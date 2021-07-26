/******************************************************************************
 * Name         : codegen.c
 * Author       : James McCarthy
 * Created      : 07/11/2005
 *
 * Copyright    : 2000-2008 by Imagination Technologies Limited.
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
 * Modifications:-
 * $Log: codegen.c $
 *****************************************************************************/

#include <stdio.h>
#include <stdarg.h>

#include "apidefs.h"
#include "codegen.h"
#include "macros.h"
#include "reg.h"
#include "inst.h"
#include "source.h"
	#include "lighting.h"
#include "uscfns.h"

#if defined(DEBUG)

#define STRIP_ENABLE1(Enable1) ui32FFTNLEnables1 &= ~(Enable1);
#define STRIP_ENABLE2(Enable2) ui32FFTNLEnables2 &= ~(Enable2);

#else

#define STRIP_ENABLE1(Enable1)
#define STRIP_ENABLE2(Enable2)

#endif

#if defined(OGLES1_MODULE)
		#define FFTNL_NUM_OUTPUT_REGISTERS 8
#else
	#define FFTNL_NUM_OUTPUT_REGISTERS 10
#endif

/*
	On chips with vectorised USSEs the fog output is padded up to 64bits to
	ensure the alignment of other elements is kept to 64bit boundaries.
*/
#if defined(SGX_FEATURE_USE_VEC34)
#define FOGDIM 2
#else
#define FOGDIM 1
#endif

/* Order in which output registers have to be allocated */
#if defined(OGLES1_MODULE)

#if defined(FIX_HW_BRN_25211)
static const FFGenRegDesc g_auOutputRegisterOrder_BRN25211[FFTNL_NUM_OUTPUT_REGISTERS] = {FFGEN_OUTPUT_POSITION,
																			    FFGEN_OUTPUT_POINTSPRITE,
																			    FFGEN_OUTPUT_TEXCOORD,

																			    FFGEN_OUTPUT_FRONTCOLOR,
																			    FFGEN_OUTPUT_BACKCOLOR,

																				#ifdef SGX545
																			    FFGEN_OUTPUT_FOGFRAGCOORD,
																			    FFGEN_OUTPUT_CLIPVERTEX,
																				FFGEN_OUTPUT_POINTSIZE,
																				#else
																			    FFGEN_OUTPUT_POINTSIZE,
																			    FFGEN_OUTPUT_FOGFRAGCOORD,
																			    FFGEN_OUTPUT_CLIPVERTEX,
																				#endif
																			};
#endif /* defined(FIX_HW_BRN_25211) */

static const FFGenRegDesc g_auOutputRegisterOrder[FFTNL_NUM_OUTPUT_REGISTERS] = {FFGEN_OUTPUT_POSITION,

																			    FFGEN_OUTPUT_FRONTCOLOR,
																			    FFGEN_OUTPUT_BACKCOLOR,
																		    
																			    #if defined(SGX_FEATURE_USE_VEC34)
																			    FFGEN_OUTPUT_FOGFRAGCOORD,
																			    #endif
																			    
																			    FFGEN_OUTPUT_POINTSPRITE,
																			    FFGEN_OUTPUT_TEXCOORD,
																			    
																			    #if defined(SGX_FEATURE_USE_VEC34)
																				FFGEN_OUTPUT_POINTSIZE,
																			    FFGEN_OUTPUT_CLIPVERTEX,
																			    #else
																				#ifdef SGX545
																			    FFGEN_OUTPUT_FOGFRAGCOORD,
																			    FFGEN_OUTPUT_CLIPVERTEX,
																				FFGEN_OUTPUT_POINTSIZE,
																				#else
																			    FFGEN_OUTPUT_POINTSIZE,
																			    FFGEN_OUTPUT_FOGFRAGCOORD,
																			    FFGEN_OUTPUT_CLIPVERTEX,
																				#endif
																				#endif
																				
																			};

#else
static const FFGenRegDesc g_auOutputRegisterOrder[FFTNL_NUM_OUTPUT_REGISTERS] = {FFGEN_OUTPUT_POSITION,
																			    FFGEN_OUTPUT_FRONTCOLOR,
																			    FFGEN_OUTPUT_FRONTSECONDARYCOLOR,
																			    
																			    #if defined(SGX_FEATURE_USE_VEC34)
																			    FFGEN_OUTPUT_FOGFRAGCOORD,
																			    #endif
																			    
																			    FFGEN_OUTPUT_POINTSPRITE,
																			    FFGEN_OUTPUT_TEXCOORD,
																			    FFGEN_OUTPUT_BACKCOLOR,
																			    FFGEN_OUTPUT_BACKSECONDARYCOLOR,
																			    
																			    #if defined(SGX_FEATURE_USE_VEC34)
																				FFGEN_OUTPUT_POINTSIZE,
																			    FFGEN_OUTPUT_CLIPVERTEX,
																			    #else
																				#ifdef SGX545
																			    FFGEN_OUTPUT_FOGFRAGCOORD,
																			    FFGEN_OUTPUT_CLIPVERTEX,
																				FFGEN_OUTPUT_POINTSIZE,
																				#else
																			    FFGEN_OUTPUT_POINTSIZE,
																			    FFGEN_OUTPUT_FOGFRAGCOORD,
																			    FFGEN_OUTPUT_CLIPVERTEX,
																				#endif
																				#endif

																			};
#endif

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
static IMG_VOID FFTNLCalculateTexCoordDimensions(FFGenCode *psFFGenCode, IMG_BOOL bAllocRegs)
{
	/* Get the FF description */
	FFTNLGenDesc *psFFTNLGenDesc            = psFFGenCode->psFFTNLGenDesc;

	IMG_UINT32 uUnitCount = 0;

	IMG_UINT32 uOutputTexCoordSize = 0;

	/* Get an amalgamantion of all enabled texture coords */
	IMG_UINT32 uEnabledCoords = (psFFTNLGenDesc->uEnabledPassthroughCoords   |
								 psFFTNLGenDesc->uEnabledEyeLinearCoords     |
								 psFFTNLGenDesc->uEnabledObjLinearCoords     |
								 psFFTNLGenDesc->uEnabledSphereMapCoords     |
								 psFFTNLGenDesc->uEnabledNormalMapCoords     |
								 psFFTNLGenDesc->uEnabledReflectionMapCoords |
								 psFFTNLGenDesc->uEnabledTextureMatrices);


	/* Work out size of input and output regs required */
	while (uEnabledCoords)
	{
		IMG_UINT32 uInputDimension = 0, uOutputDimension = 0;

		IMG_UINT32 uPassthroughCoordMask = psFFTNLGenDesc->aubPassthroughCoordMask[uUnitCount];

		/* Work out dimension of input */
		while (uPassthroughCoordMask)
		{
			uPassthroughCoordMask >>= 1;

			uInputDimension++;
		}

		/* Work out dimension of output */
		if (psFFTNLGenDesc->uEnabledTextureMatrices & (0x1 << uUnitCount))
		{
			uOutputDimension = 4;
		}
		else
		{
			/* Get an amalgamation of all enabled coordinate masks */
			IMG_UINT32 uTotalCoordMask = (psFFTNLGenDesc->aubPassthroughCoordMask[uUnitCount] |
										  psFFTNLGenDesc->aubEyeLinearCoordMask[uUnitCount]   |
										  psFFTNLGenDesc->aubObjLinearCoordMask[uUnitCount]   |
										  psFFTNLGenDesc->aubSphereMapCoordMask[uUnitCount]   |
										  psFFTNLGenDesc->aubNormalMapCoordMask[uUnitCount]   |
										  psFFTNLGenDesc->aubPositionMapCoordMask[uUnitCount]   |
										  psFFTNLGenDesc->aubReflectionMapCoordMask[uUnitCount]);

			/* Work out dimension of output */
			while (uTotalCoordMask)
			{
				uTotalCoordMask >>= 1;

				uOutputDimension++;
			}
		}

		/* Quick sanity check */
		if (uInputDimension > 4 || uOutputDimension >> 4)
		{
			psFFGenCode->psFFGenContext->pfnPrint("FFTNLGenTexturing: Dimension of tex coordinate is greater than 4!\n");
		}

		/* Store dimension of this output texture coordinate */
		psFFGenCode->auInputTexDimensions[uUnitCount]  = uInputDimension;
		psFFGenCode->auOutputTexDimensions[uUnitCount] = uOutputDimension;

		/* Increase offset */
		uOutputTexCoordSize += uOutputDimension;

		/* Shift mask */
		uEnabledCoords >>= 1;

		/* Increase unit count */
		uUnitCount++;
	}

	psFFGenCode->uNumTexCoordUnits = uUnitCount;

	if (bAllocRegs)
	{
		/* Create array for output texture coordinates */
		GetReg(psFFGenCode,
			   USEASM_REGTYPE_OUTPUT,
			   FFGEN_OUTPUT_TEXCOORD,
			   0,
			   uOutputTexCoordSize,
			   IMG_NULL);
	}
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
static IMG_VOID FFTNLCalculateClipPlaneRequirements(FFGenCode *psFFGenCode, IMG_BOOL bAllocRegs)
{
	/* Get the FF description */
	FFTNLGenDesc *psFFTNLGenDesc            = psFFGenCode->psFFTNLGenDesc;

	IMG_UINT32 uEnabledClipPlanes = psFFTNLGenDesc->uEnabledClipPlanes;
	
	/* count number of clip planes */
	while (uEnabledClipPlanes)
	{
		if (uEnabledClipPlanes & 0x1)
		{
			psFFGenCode->uNumOutputClipPlanes++;
		}
		
		psFFGenCode->uNumConstantClipPlanes++;
		
		uEnabledClipPlanes >>= 1;
	}
	
	if (bAllocRegs)
	{
		/* get output reg for clipping results */
		GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_CLIPVERTEX, 0, psFFGenCode->uNumOutputClipPlanes, IMG_NULL);
	}
}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Output registers need to be defined in a certain order  
 *****************************************************************************/
static IMG_VOID FFTNLAssignOutputRegisters(FFGenCode *psFFGenCode)
{
	/* Get the FF description */
	FFTNLGenDesc *psFFTNLGenDesc            = psFFGenCode->psFFTNLGenDesc;
	
	/* Get the enables */
	IMG_UINT32 ui32FFTNLEnables1 = psFFTNLGenDesc->ui32FFTNLEnables1;
	IMG_UINT32 ui32FFTNLEnables2 = psFFTNLGenDesc->ui32FFTNLEnables2;

	IMG_UINT32 i;

	/* If we're redirecting the writes we don't want any output registers */
	if (psFFGenCode->eCodeGenFlags & FFGENCGF_REDIRECT_OUTPUT_TO_INPUT)
	{
		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_TEXTURINGENABLED)
		{
			FFTNLCalculateTexCoordDimensions(psFFGenCode, IMG_FALSE);
		}

		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_CLIPPING)
		{
			/* Get output clip plane regs */
			FFTNLCalculateClipPlaneRequirements(psFFGenCode, IMG_FALSE);
		}
			
		return;
	}

#if defined(FIX_HW_BRN_25211) && defined(OGLES1_MODULE)
	if(ui32FFTNLEnables2 & FFTNL_ENABLES2_CLAMP_OUTPUT_COLOURS)
	{
		/* Allocate registers in correct order */
		for (i = 0; i < FFTNL_NUM_OUTPUT_REGISTERS; i++)
		{
			switch (g_auOutputRegisterOrder_BRN25211[i])
			{
				case FFGEN_OUTPUT_POSITION:
				{
					/* Get position register */
					GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_POSITION, 0, 4, IMG_NULL);
					break;
				}
				case FFGEN_OUTPUT_FRONTCOLOR:
				{
					/* get output colour reg */
					GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTCOLOR, 0, 4, IMG_NULL);
					break;
				}
				case FFGEN_OUTPUT_FRONTSECONDARYCOLOR:
				{
					if (ui32FFTNLEnables1 & (FFTNL_ENABLES1_SECONDARYCOLOUR | FFTNL_ENABLES1_SEPARATESPECULAR))
					{
						/* get output colour reg */
						GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTSECONDARYCOLOR, 0, 4, IMG_NULL);
					}
					break;
				}
				case FFGEN_OUTPUT_POINTSPRITE:
				{
					if (ui32FFTNLEnables2 & FFTNL_ENABLES2_POINTSPRITES)
					{
						/* get output colour reg */
						GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_POINTSPRITE, 0, 2, IMG_NULL);
					}
					break;
				}
				case FFGEN_OUTPUT_TEXCOORD:
				{
					if (ui32FFTNLEnables1 & FFTNL_ENABLES1_TEXTURINGENABLED)
					{
						FFTNLCalculateTexCoordDimensions(psFFGenCode, IMG_TRUE);
					}
					break;
				}
				case FFGEN_OUTPUT_BACKCOLOR:
				{
					/* Back facing colour registers - loaded into textire coordinates so they go here */
					if (ui32FFTNLEnables1 & FFTNL_ENABLES1_LIGHTINGTWOSIDED)
					{
						/* get output colour reg */
						GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_BACKCOLOR, 0, 4, IMG_NULL);
					}
					break;
				}
				case FFGEN_OUTPUT_BACKSECONDARYCOLOR:
				{
					/* Back facing colour registers - loaded into textire coordinates so they go here */
					if (ui32FFTNLEnables1 & FFTNL_ENABLES1_LIGHTINGTWOSIDED)
					{
						if (ui32FFTNLEnables1 & FFTNL_ENABLES1_SEPARATESPECULAR)
						{
							/* get output colour reg */
							GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_BACKSECONDARYCOLOR, 0, 4, IMG_NULL);
						}
					}
					break;
				}
				case FFGEN_OUTPUT_POINTSIZE:
				{
					if (ui32FFTNLEnables2 & (FFTNL_ENABLES2_POINTS|FFTNL_ENABLES2_POINTSPRITES))
					{
						/* get output point size reg */
						GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_POINTSIZE, 0, 1, IMG_NULL);
					}
					break;
				}
				case FFGEN_OUTPUT_FOGFRAGCOORD:
				{
					if (ui32FFTNLEnables1 & (FFTNL_ENABLES1_FOGCOORD       |
										 FFTNL_ENABLES1_FOGCOORDEYEPOS |
										 FFTNL_ENABLES1_FOGLINEAR      |
										 FFTNL_ENABLES1_FOGEXP         |
										 FFTNL_ENABLES1_FOGEXP2))
					{
						/* Get output fog register */
						GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FOGFRAGCOORD, 0, FOGDIM, IMG_NULL);
					}
					break;
				}
				case FFGEN_OUTPUT_CLIPVERTEX:	
				{
					if (ui32FFTNLEnables1 & FFTNL_ENABLES1_CLIPPING)
					{
						/* Get output clip plane regs */
						FFTNLCalculateClipPlaneRequirements(psFFGenCode, IMG_TRUE);
					}
					break;
				}
				default:
				{
					psFFGenCode->psFFGenContext->pfnPrint("Unrecogised output register %d\n", g_auOutputRegisterOrder_BRN25211[i]);
					break;
				}
			}
		}
	}
	else
#endif /* defined(FIX_HW_BRN_25211) && defined(OGLES1_MODULE) */
	{
		/* Allocate registers in correct order */
		for (i = 0; i < FFTNL_NUM_OUTPUT_REGISTERS; i++)
		{
			switch (g_auOutputRegisterOrder[i])
			{
				case FFGEN_OUTPUT_POSITION:
				{
					/* Get position register */
					GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_POSITION, 0, 4, IMG_NULL);
					break;
				}
				case FFGEN_OUTPUT_FRONTCOLOR:
				{
					/* get output colour reg */
					GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTCOLOR, 0, 4, IMG_NULL);
					break;
				}
				case FFGEN_OUTPUT_FRONTSECONDARYCOLOR:
				{
					if (ui32FFTNLEnables1 & (FFTNL_ENABLES1_SECONDARYCOLOUR | FFTNL_ENABLES1_SEPARATESPECULAR))
					{
						/* get output colour reg */
						GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTSECONDARYCOLOR, 0, 4, IMG_NULL);
					}
					break;
				}
				case FFGEN_OUTPUT_POINTSPRITE:
				{
					if (ui32FFTNLEnables2 & FFTNL_ENABLES2_POINTSPRITES)
					{
						/* get output colour reg */
						GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_POINTSPRITE, 0, 2, IMG_NULL);
					}
					break;
				}
				case FFGEN_OUTPUT_TEXCOORD:
				{
					if (ui32FFTNLEnables1 & FFTNL_ENABLES1_TEXTURINGENABLED)
					{
						FFTNLCalculateTexCoordDimensions(psFFGenCode, IMG_TRUE);
					}
					break;
				}
				case FFGEN_OUTPUT_BACKCOLOR:
				{
					/* Back facing colour registers - loaded into textire coordinates so they go here */
					if (ui32FFTNLEnables1 & FFTNL_ENABLES1_LIGHTINGTWOSIDED)
					{
						/* get output colour reg */
						GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_BACKCOLOR, 0, 4, IMG_NULL);
					}
					break;
				}
				case FFGEN_OUTPUT_BACKSECONDARYCOLOR:
				{
					/* Back facing colour registers - loaded into textire coordinates so they go here */
					if (ui32FFTNLEnables1 & FFTNL_ENABLES1_LIGHTINGTWOSIDED)
					{
						if (ui32FFTNLEnables1 & FFTNL_ENABLES1_SEPARATESPECULAR)
						{
							/* get output colour reg */
							GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_BACKSECONDARYCOLOR, 0, 4, IMG_NULL);
						}
					}
					break;
				}
				case FFGEN_OUTPUT_POINTSIZE:
				{
					if (ui32FFTNLEnables2 & (FFTNL_ENABLES2_POINTS|FFTNL_ENABLES2_POINTSPRITES))
					{
						/* get output point size reg */
						GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_POINTSIZE, 0, 1, IMG_NULL);
					}
					break;
				}
				case FFGEN_OUTPUT_FOGFRAGCOORD:
				{
					if (ui32FFTNLEnables1 & (FFTNL_ENABLES1_FOGCOORD       |
										 FFTNL_ENABLES1_FOGCOORDEYEPOS |
										 FFTNL_ENABLES1_FOGLINEAR      |
										 FFTNL_ENABLES1_FOGEXP         |
										 FFTNL_ENABLES1_FOGEXP2))
					{
						/* Get output fog register */
						GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FOGFRAGCOORD, 0, FOGDIM, IMG_NULL);
					}
					break;
				}
				case FFGEN_OUTPUT_CLIPVERTEX:	
				{
					if (ui32FFTNLEnables1 & FFTNL_ENABLES1_CLIPPING)
					{
						/* Get output clip plane regs */
						FFTNLCalculateClipPlaneRequirements(psFFGenCode, IMG_TRUE);
					}
					break;
				}
				default:
				{
					psFFGenCode->psFFGenContext->pfnPrint("Unrecogised output register %d\n", g_auOutputRegisterOrder[i]);
					break;
				}
			}
		}
	}
}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Output registers need to be defined in a certain order  
 *****************************************************************************/
static IMG_VOID FFTNLRedirectOutputRegisterWrites(FFGenCode *psFFGenCode)
{
	IMG_UINT32 i;
	IMG_UINT32 uCurrentOutputSize = 0;

	if (!psFFGenCode->psOutputsList)
	{
		psFFGenCode->psFFGenContext->pfnPrint("FFTNLRedirectOutputRegisterWrites: No regs in output list!");
		return;
	}
#if defined(FIX_HW_BRN_25211) && defined(OGLES1_MODULE)
	if(psFFGenCode->psFFTNLGenDesc->ui32FFTNLEnables2 & FFTNL_ENABLES2_CLAMP_OUTPUT_COLOURS)
	{
		/* Allocate registers in correct order */
		for (i = 0; i < FFTNL_NUM_OUTPUT_REGISTERS; i++)
		{
			FFGenRegList *psRegList = psFFGenCode->psOutputsList;
			
			/* Loop through list in correct order */
			while (psRegList)		
			{
				FFGenReg *psOutputReg = psRegList->psReg;
				
				if (psOutputReg->eBindingRegDesc == g_auOutputRegisterOrder_BRN25211[i])
				{
					FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

					FFGenReg sInputReg = *psOutputReg;

					IMG_UINT32 uNumRegsCopied = 0;

#if defined(STANDALONE) || defined(DEBUG)
					/* quick sanity check */
					if (psOutputReg->eType != USEASM_REGTYPE_TEMP)
					{
						psFFGenCode->psFFGenContext->pfnPrint("FFTNLRedirectOutputRegisterWrites: Output register was not temp (%08x)\n", 
										  psOutputReg->eType) ;
					}
#endif
					while (uNumRegsCopied != psOutputReg->uSizeInDWords)
					{
						/* Setup input reg */
						sInputReg.eType   = USEASM_REGTYPE_PRIMATTR;
						sInputReg.uOffset = uCurrentOutputSize + uNumRegsCopied;

						/* Offset output register */
						psOutputReg->uOffset += uNumRegsCopied;
						
						if (psOutputReg->uSizeInDWords - uNumRegsCopied > 16)
						{
							SET_REPEAT_COUNT(16);
							INST1(MOV, &sInputReg, psOutputReg, "Redirect output reg to input reg");
							uNumRegsCopied += 16;
						}
						else
						{
							/* Insert instruction to copy register */
							SET_REPEAT_COUNT((psOutputReg->uSizeInDWords - uNumRegsCopied));
							INST1(MOV, &sInputReg, psOutputReg, "Redirect output reg to input reg");
							uNumRegsCopied += psOutputReg->uSizeInDWords - uNumRegsCopied;
						}
					}				

					/* Change register info */
					psOutputReg->eType   = USEASM_REGTYPE_OUTPUT;
					psOutputReg->uOffset = uCurrentOutputSize; 

					uCurrentOutputSize += psOutputReg->uSizeInDWords;
					break;
				}

				/* Get next register */
				psRegList = psRegList->psNext;
			}
		}
	}
	else
#endif /* defined(FIX_HW_BRN_25211) && defined(OGLES1_MODULE) */
	{
		/* Allocate registers in correct order */
		for (i = 0; i < FFTNL_NUM_OUTPUT_REGISTERS; i++)
		{
			FFGenRegList *psRegList = psFFGenCode->psOutputsList;
			
			/* Loop through list in correct order */
			while (psRegList)		
			{
				FFGenReg *psOutputReg = psRegList->psReg;
				
				if (psOutputReg->eBindingRegDesc == g_auOutputRegisterOrder[i])
				{
					FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

					FFGenReg sInputReg = *psOutputReg;

					IMG_UINT32 uNumRegsCopied = 0;

	#if defined(STANDALONE) || defined(DEBUG)
					/* quick sanity check */
					if (psOutputReg->eType != USEASM_REGTYPE_TEMP)
					{
						psFFGenCode->psFFGenContext->pfnPrint("FFTNLRedirectOutputRegisterWrites: Output register was not temp (%08x)\n", 
										  psOutputReg->eType) ;
					}
	#endif
					while (uNumRegsCopied != psOutputReg->uSizeInDWords)
					{
						/* Setup input reg */
						sInputReg.eType   = USEASM_REGTYPE_PRIMATTR;
						sInputReg.uOffset = uCurrentOutputSize + uNumRegsCopied;

						/* Offset output register */
						psOutputReg->uOffset += uNumRegsCopied;
						
						if (psOutputReg->uSizeInDWords - uNumRegsCopied > 16)
						{
							SET_REPEAT_COUNT(16);
							INST1(MOV, &sInputReg, psOutputReg, "Redirect output reg to input reg");
							uNumRegsCopied += 16;
						}
						else
						{
							/* Insert instruction to copy register */
							SET_REPEAT_COUNT((psOutputReg->uSizeInDWords - uNumRegsCopied));
							INST1(MOV, &sInputReg, psOutputReg, "Redirect output reg to input reg");
							uNumRegsCopied += psOutputReg->uSizeInDWords - uNumRegsCopied;
						}
					}				

					/* Change register info */
					psOutputReg->eType   = USEASM_REGTYPE_OUTPUT;
					psOutputReg->uOffset = uCurrentOutputSize; 

					uCurrentOutputSize += psOutputReg->uSizeInDWords;
					break;
				}

				/* Get next register */
				psRegList = psRegList->psNext;
			}
		}
	}

}


/******************************************************************************
 * Function Name: FFTNLGenTransformation
 * Inputs       : psFFGenCode
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
static IMG_VOID FFTNLGenTransformation(FFGenCode *psFFGenCode)
{
	/* Get the FF description */
	FFTNLGenDesc *psFFTNLGenDesc = psFFGenCode->psFFTNLGenDesc;



	/* Get the transformation enables */
	IMG_UINT32 ui32FFTNLEnables1 = psFFTNLGenDesc->ui32FFTNLEnables1 & (FFTNL_ENABLES1_STANDARDTRANSFORMATION  |
															      FFTNL_ENABLES1_VERTEXBLENDING          |
															      FFTNL_ENABLES1_MATRIXPALETTEBLENDING   |
															      FFTNL_ENABLES1_TRANSFORMNORMALS        |
															      FFTNL_ENABLES1_NORMALISENORMALS        |
															      FFTNL_ENABLES1_USEMVFORNORMALTRANSFORM |
															      FFTNL_ENABLES1_EYEPOSITION);
	/* Get input and output regs */
	FFGenReg *psInputPosition   = GetReg(psFFGenCode, USEASM_REGTYPE_PRIMATTR,  FFGEN_INPUT_VERTEX,   0, 4, IMG_NULL);
	FFGenReg *psOutputPosition  = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_POSITION, 0, 4, IMG_NULL);
	FFGenReg *psInputNormal     = IMG_NULL;
	FFGenReg *psOutputNormal    = IMG_NULL;

	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

	FFGenReg *psMatrix = IMG_NULL;
	
	/* Can we reuse the MV matrix for transforming our normals? */
	IMG_BOOL bReuseMVMatrixForNormals = ((ui32FFTNLEnables1 & FFTNL_ENABLES1_TRANSFORMNORMALS) && 
										 (ui32FFTNLEnables1 & FFTNL_ENABLES1_USEMVFORNORMALTRANSFORM));

	if (ui32FFTNLEnables1 & FFTNL_ENABLES1_TRANSFORMNORMALS)
	{
		/* Get input and output normal */
		psInputNormal  = GetReg(psFFGenCode, USEASM_REGTYPE_PRIMATTR,  FFGEN_INPUT_NORMAL, 0, 4, IMG_NULL);
#if defined(FFGEN_UNIFLEX)
		psOutputNormal = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP,   0,                  0, 4, IMG_NULL);
#else
		psOutputNormal = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP,   0,                  0, 3, IMG_NULL);
#endif /* defined(FFGEN_UNIFLEX) */
	}

	/* Store for later use */
	psFFGenCode->psNormal = psOutputNormal;

	if (ui32FFTNLEnables1 & FFTNL_ENABLES1_STANDARDTRANSFORMATION)
	{
		#if defined(DEBUG)
		
			/* Check for illegal setup of enables */
			if (ui32FFTNLEnables1 & (FFTNL_ENABLES1_VERTEXBLENDING | FFTNL_ENABLES1_MATRIXPALETTEBLENDING))
			{
				psFFGenCode->psFFGenContext->pfnPrint("Illegal setup of transformation enables? (%08X)\n", ui32FFTNLEnables1);
			}
		
		#endif
		
		/*
		  LOAD_CONST(r[tT1], 16, cMVPRow0, 0) ; 
		  m4x4(o, oPosition, pa, vPosition, r, tT1) ;
		*/
	
		NEW_BLOCK(psFFGenCode, "Standard Transformation");

		/* Load the MVP */
		psMatrix = GetReg(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_MODELVIEWPROJECTIONMATRIX, 0, FFTNL_SIZE_MATRIX_4X4, "MVP Matrix");

		/* Transform by position by MPV */
		M4x4(psFFGenCode, psOutputPosition, psInputPosition, psMatrix, 0, "Transform the the position by the MVP");

		/* Release matrix if not being reused */
		if (!bReuseMVMatrixForNormals)
		{
			ReleaseReg(psFFGenCode, psMatrix);
		}

		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_TRANSFORMNORMALS)
		{
#if defined(FFGEN_UNIFLEX)
			FFGenReg *psImmediateFloatReg = &(psFFGenCode->sImmediateFloatReg);
#endif /* defined(FFGEN_UNIFLEX) */
			
			NEW_BLOCK(psFFGenCode, "Transform Normals");

			if (!bReuseMVMatrixForNormals)
			{
				psMatrix = GetReg(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSE, 0, FFTNL_SIZE_MATRIX_4X3, "ITMV matrix");
			}

			/* Transform by position by MV */
			M4x3(psFFGenCode, psOutputNormal, psInputNormal, psMatrix, "Transform the the normal by the inverse transpose MV");

#if defined(FFGEN_UNIFLEX)
			/* hard code w to 1.0f */
			SETDESTOFF(3); SET_FLOAT_ONE(psImmediateFloatReg);
			INST1(MOV, psOutputNormal, psImmediateFloatReg, IMG_NULL);
#endif /* defined(FFGEN_UNIFLEX) */
			
			/* Release MV constant */
			ReleaseReg(psFFGenCode, psMatrix);
		}

		STRIP_ENABLE1(FFTNL_ENABLES1_STANDARDTRANSFORMATION | 
					 FFTNL_ENABLES1_TRANSFORMNORMALS       |
					 FFTNL_ENABLES1_USEMVFORNORMALTRANSFORM);
	}
	else if (ui32FFTNLEnables1 & (FFTNL_ENABLES1_VERTEXBLENDING | FFTNL_ENABLES1_MATRIXPALETTEBLENDING))
	{
		IMG_UINT32 i, j;

		IMG_UINT32 uNumMatrices;

		FFGenReg *psMatrixIndices = IMG_NULL, *psInputIndices = IMG_NULL;

		/* Get temp for eye position */
		FFGenReg *psEyePosition = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 4, IMG_NULL);
		FFGenReg *psTempPosition = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 4, IMG_NULL);

		FFGenReg *psInputBlendWeight  = GetReg(psFFGenCode,
											   USEASM_REGTYPE_PRIMATTR,
											   FFGEN_INPUT_VERTEXBLENDWEIGHT,
											   0,
											   psFFTNLGenDesc->uNumBlendUnits,
											   IMG_NULL);

		FFGENIndexReg sIndexReg;

		/* Work out how many matrices are required */
		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_MATRIXPALETTEBLENDING)
		{
			/* Get input palette indices */
			psInputIndices	= GetReg(psFFGenCode,
									   USEASM_REGTYPE_PRIMATTR,
									   FFGEN_INPUT_VERTEXMATRIXINDEX,
									   0,
									   psFFTNLGenDesc->uNumBlendUnits,
									   IMG_NULL);
			
			uNumMatrices = psFFTNLGenDesc->uNumMatrixPaletteEntries;

			/* Allocate space for the matrices */
			AllocRegSpace(psFFGenCode,
						  USEASM_REGTYPE_SECATTR,
						  FFGEN_STATE_MODELVIEWMATRIXPALETTE,
						  uNumMatrices * FFTNL_SIZE_MATRIX_4X4,
						  IMG_TRUE);

			/* Allocate space for normal matrices */
			if ((ui32FFTNLEnables1 & FFTNL_ENABLES1_TRANSFORMNORMALS) && !bReuseMVMatrixForNormals)
			{
				AllocRegSpace(psFFGenCode,
							  USEASM_REGTYPE_SECATTR,
							  FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSEPALETTE,
							  uNumMatrices * FFTNL_SIZE_MATRIX_4X4,
							  IMG_TRUE);
			}

#ifdef FFGEN_UNIFLEX
			if(!IF_FFGENCODE_UNIFLEX)
#endif
			{				
				FFGenReg *psPredReg0        = &(psFFGenCode->asPredicateRegs[0]);
				FFGenReg *psIntSrcSelReg 	= &psFFGenCode->sIntSrcSelReg;				
				FFGenReg *psIndexClampValue = GetReg(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_MATRIXPALETTEINDEXCLAMP, 0, 1, "Matrix index clamp value");
			
				/* Get reg for matrix palette indices */
				psMatrixIndices =  GetReg(psFFGenCode,
					USEASM_REGTYPE_TEMP,
					0,
					0,
					(psFFTNLGenDesc->uNumBlendUnits + 1) / 2,
					IMG_NULL);

				sIndexReg.psReg = psMatrixIndices;
				
				NEW_BLOCK(psFFGenCode, "Matrix palette Vertex Blending");

				COMMENT(psFFGenCode, "\n      Reuse of MV matrix for normal transformation is enabled.\n      Disable if things start looking wrong\n   ");

				COMMENT(psFFGenCode, "\n      Clamp matrix indices \n   ");				

				for (i=0; i<psFFTNLGenDesc->uNumBlendUnits; i++)
				{
					/*
					  fsub.testp p0, pa[matrix index], sa[clamp value]; 
					*/
					SET_TESTPRED_POSITIVE();
					SETSRCOFF(0, i);
					INST2(FSUB, psPredReg0, psInputIndices, psIndexClampValue, IMG_NULL);

					/*
					  p0 mov pa[matrix index], sa[clamp value];
					*/
					USE_PRED(psPredReg0);
					SETDESTOFF(i)
					INST1(MOV, psInputIndices, psIndexClampValue, IMG_NULL);
				}
		
				ReleaseReg(psFFGenCode, psIndexClampValue);

				/* Set round mode to be nearest for pack inst */
				SET_INTSRCSEL(psIntSrcSelReg, USEASM_INTSRCSEL_ROUNDNEAREST);

				
				/* Unpack all the matrix indices */
				for (i = 0; i < (psFFTNLGenDesc->uNumBlendUnits + 1) / 2; i++)
				{
					/* Not sure why but have to setup mask for pack instructions */
					SETDESTMASK(0xF);
					/* Pack the floating point indices to 16 bit integers */
					SETDESTOFF(i); SETSRCOFF(0, (i * 2) + 0); SETSRCOFF(1, (i * 2) + 1);  
					INST3(PCKU16F32, psMatrixIndices, psInputIndices, psInputIndices, psIntSrcSelReg, "Unpack indices");
				}
			} /* if(!IF_FFGENCODE_UNIFLEX) */
		} /* if (ui32FFTNLEnables1 & FFTNL_ENABLES1_MATRIXPALETTEBLENDING) */
		else
		{
			uNumMatrices = psFFTNLGenDesc->uNumBlendUnits;

			/* Allocate space for the matrices */
			AllocRegSpace(psFFGenCode,
						  USEASM_REGTYPE_SECATTR,
						  FFGEN_STATE_MODELVIEWMATRIX,
						  uNumMatrices * FFTNL_SIZE_MATRIX_4X4, 
						  IMG_FALSE);

			/* Allocate space for normal matrices */
			if ((ui32FFTNLEnables1 & FFTNL_ENABLES1_TRANSFORMNORMALS) && !bReuseMVMatrixForNormals)
			{
				AllocRegSpace(psFFGenCode,
							  USEASM_REGTYPE_SECATTR,
							  FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSE,
							  uNumMatrices * FFTNL_SIZE_MATRIX_4X4, 
							  IMG_FALSE);
			}

			NEW_BLOCK(psFFGenCode, "Vertex Blending");

		}

		for (i = 0; i < psFFTNLGenDesc->uNumBlendUnits; i++)
		{
			COMMENT(psFFGenCode, "\n      ------ Matrix %d ------\n   ", i);
			
			if (ui32FFTNLEnables1 & FFTNL_ENABLES1_VERTEXBLENDING)
			{
				/* Load the next matrix */
				psMatrix = GetReg(psFFGenCode,
								  USEASM_REGTYPE_SECATTR,
								  FFGEN_STATE_MODELVIEWMATRIX,
								  i * FFTNL_SIZE_MATRIX_4X4,
								  FFTNL_SIZE_MATRIX_4X4,
								  "MV matrix");
			}
			else
			{
#ifdef FFGEN_UNIFLEX
				if(IF_FFGENCODE_UNIFLEX)
				{					
					/* Set the address register to index the correct matrix from the palettes */

					FFGenReg sDest = {0};
					FFGenReg *psIndexClampValue = GetReg(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_MATRIXPALETTEINDEXCLAMP, 0, 1, IMG_NULL);	
					FFGenReg *psTemp			= GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);
					FFGenReg *psPredReg0        = &(psFFGenCode->asPredicateRegs[0]);

					/* Move the index to the temp */
					SETDESTOFF(0); SETSRCOFF(0, i);
					INST1(MOV, psTemp, psInputIndices, IMG_NULL);

					/* Test if index is out of range */
					SET_TESTPRED_NEGATIVE();
					SETSRCOFF(0, 0); SETSRCOFF(1, 0);
					INST2(FSUB, psPredReg0, psIndexClampValue, psTemp, IMG_NULL);

					/* Clamp it if necessary */
					USE_PRED(psPredReg0);
					SETDESTOFF(0); SETSRCOFF(0, 0);
					INST1(MOV, psTemp, psIndexClampValue, IMG_NULL);
					
					/* Store the offset in the relative address register */
					sDest.eType = UFREG_TYPE_ADDRESS;
					SETDESTOFF(0); SETSRCOFF(0, 0);
					INST1(MOV, &sDest, psTemp, IMG_NULL);

					ReleaseReg(psFFGenCode, psIndexClampValue);
					ReleaseReg(psFFGenCode, psTemp);
				}				
#endif				
				/* Set up the index register */
				if (i & 0x1)
				{
					sIndexReg.uIndexRegFlags = USEASM_ARGFLAGS_HIGH;
				}
				else
				{
					sIndexReg.uIndexRegFlags = USEASM_ARGFLAGS_LOW;
				}


				/* Set up the index register offset */
				sIndexReg.uIndexRegOffset = i / 2;
				sIndexReg.uStrideInDWords = FFTNL_SIZE_MATRIX_4X4;

				/* Load the next matrix */
				psMatrix = GetIndexedReg(psFFGenCode,
										 USEASM_REGTYPE_SECATTR,
										 FFGEN_STATE_MODELVIEWMATRIXPALETTE,
										 &sIndexReg,
										 FFTNL_SIZE_MATRIX_4X4,
										 "MV matrix palette");
			}

			/* Transform by position by MV */
			M4x4(psFFGenCode, psTempPosition, psInputPosition, psMatrix, 0, "Transform the the position by the MV");

			if (i)
			{
				COMMENT(psFFGenCode, "Multiply position by blend weight and accumulate");

				for (j = 0; j < 4; j++)
				{
					SETDESTOFF(j);
					SETSRCOFF(0, j);
					SETSRCOFF(1, i);
					SETSRCOFF(2, j);
					INST3(FMAD, psEyePosition, psTempPosition,  psInputBlendWeight, psEyePosition, IMG_NULL);
				}
			}
			else
			{
				COMMENT(psFFGenCode, "Multiply position by blend weight");

				for (j = 0; j < 4; j++)
				{
					SETDESTOFF(j);
					SETSRCOFF(0, j);
					SETSRCOFF(1, i);
					INST2(FMUL, psEyePosition, psTempPosition,  psInputBlendWeight, IMG_NULL);
					
				}
			}

			if (!bReuseMVMatrixForNormals)
			{
				/* Release MV constant */
				ReleaseReg(psFFGenCode, psMatrix);
			}

			if (ui32FFTNLEnables1 & FFTNL_ENABLES1_TRANSFORMNORMALS)
			{
				if (!bReuseMVMatrixForNormals)
				{
					if (ui32FFTNLEnables1 & FFTNL_ENABLES1_VERTEXBLENDING)
					{
						psMatrix = GetReg(psFFGenCode,
										  USEASM_REGTYPE_SECATTR,
										  FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSE,
										  i * FFTNL_SIZE_MATRIX_4X4,
										  FFTNL_SIZE_MATRIX_4X3,
										  "ITMV matrix");
					}
					else
					{
						/* Set up the index register */
						if (i & 0x1)
						{
							sIndexReg.uIndexRegFlags = USEASM_ARGFLAGS_HIGH;
						}
						else
						{
							sIndexReg.uIndexRegFlags = USEASM_ARGFLAGS_LOW;
						}
						
						
						/* Set up the index register offset */
						sIndexReg.uIndexRegOffset = i / 2;
						sIndexReg.uStrideInDWords = FFTNL_SIZE_MATRIX_4X4;
						
						/* Load the next matrix */
						psMatrix = GetIndexedReg(psFFGenCode,
												 USEASM_REGTYPE_SECATTR,
												 FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSEPALETTE,
												 &sIndexReg,
												 FFTNL_SIZE_MATRIX_4X3,
												 "ITMV matrix palette");
					}
				}

				/* Transform normal  by ITMV */
				M4x3(psFFGenCode, psTempPosition, psInputNormal, psMatrix, "Transform the the normal by the ITMV");

				if (i)
				{
					COMMENT(psFFGenCode, "Multiply normal  by blend weight and accumulate");

					for (j = 0; j < 3; j++)
					{
						SETDESTOFF(j);
						SETSRCOFF(0, j);
						SETSRCOFF(1, i);
						SETSRCOFF(2, j);
						INST3(FMAD, psOutputNormal, psTempPosition,  psInputBlendWeight, psOutputNormal, IMG_NULL);
					}
				}
				else
				{
					COMMENT(psFFGenCode, "Multiply normal  by blend weight");

					for (j = 0; j < 3; j++)
					{
						SETDESTOFF(j);
						SETSRCOFF(0, j);
						SETSRCOFF(1, i);
						INST2(FMUL, psOutputNormal, psTempPosition,  psInputBlendWeight, IMG_NULL);
					}
				}			

				/* Release ITMV constant */
				ReleaseReg(psFFGenCode, psMatrix);
			}
		} /* for (i = 0; i < psFFTNLGenDesc->uNumBlendUnits; i++) */

		/* Get projection matrix */
		psMatrix = GetReg(psFFGenCode,
						  USEASM_REGTYPE_SECATTR,
						  FFGEN_STATE_PROJECTMATRIX,
						  0,
						  FFTNL_SIZE_MATRIX_4X4,
						  "Projection matrix");

		/* Transform by projection matrix */
		M4x4(psFFGenCode, psOutputPosition, psEyePosition, psMatrix, 0, "Transform the eye position by projection matrix");

		/* Store eye position for possible later use */
		psFFGenCode->psEyePosition = psEyePosition;

		if(psMatrixIndices)
		{
			ReleaseReg(psFFGenCode, psMatrixIndices);
		}

		if(psInputIndices)
		{
			ReleaseReg(psFFGenCode, psInputIndices);
		}

		/* Release temp constant */
		ReleaseReg(psFFGenCode, psTempPosition);

		STRIP_ENABLE1(FFTNL_ENABLES1_VERTEXBLENDING        | 
					 FFTNL_ENABLES1_MATRIXPALETTEBLENDING | 
					 FFTNL_ENABLES1_TRANSFORMNORMALS      |
					 FFTNL_ENABLES1_USEMVFORNORMALTRANSFORM);
	}
	else
	{
		/* Just pass the position and normal through untransformed */

		/*
		  mov.rpt4		o[oPosition],			pa[vPosition] ;
		  mov.rpt4		r[tNormal],				pa[vNormal] ;
		*/

		NEW_BLOCK(psFFGenCode, "No transformation - pass through");

		SET_REPEAT_COUNT(4);
		INST1(MOV, psOutputPosition, psInputPosition, "Pass through the position");

		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_TRANSFORMNORMALS)
		{
			SET_REPEAT_COUNT(3);
			INST1(MOV, psOutputNormal, psInputNormal, "Pass through the normal");
		}
	}

	if (ui32FFTNLEnables1 & FFTNL_ENABLES1_NORMALISENORMALS)
	{
		/*
		  fdp3				r[DP3_DST(tT1)],			r[tNormal],				r[tNormal]; 
		  frsq				r[tT1],						r[tT1];
		  fmul				r[tNormal + _X_],			r[tNormal + _X_],		r[tT1];
		  fmul				r[tNormal + _Y_],			r[tNormal + _Y_],		r[tT1];
		  fmul				r[tNormal + _Z_],			r[tNormal + _Z_],		r[tT1];
		*/

		FFGenReg *psTemp1 = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

		NEW_BLOCK(psFFGenCode, "Normalise normals");

		SETDESTOFF(DP3_ADJUST(0));
		INST2(FDP3, psTemp1, psOutputNormal, psOutputNormal, "Normalise supplied normal vector");
		INST1(FRSQ, psTemp1, psTemp1, IMG_NULL);

		SETDESTOFF(0);  SETSRCOFF(0, 0); 
		INST2(FMUL, psOutputNormal, psOutputNormal, psTemp1, IMG_NULL);

		SETDESTOFF(1);  SETSRCOFF(0, 1); 
		INST2(FMUL, psOutputNormal, psOutputNormal, psTemp1, IMG_NULL);

		SETDESTOFF(2);  SETSRCOFF(0, 2); 
		INST2(FMUL, psOutputNormal, psOutputNormal, psTemp1, IMG_NULL);

		ReleaseReg(psFFGenCode, psTemp1);

		STRIP_ENABLE1(FFTNL_ENABLES1_NORMALISENORMALS);
	}

	if (ui32FFTNLEnables1 & FFTNL_ENABLES1_EYEPOSITION)
	{
		/* May already have been calculated by blending code, if not calculate it */
		if (!psFFGenCode->psEyePosition)
		{
			/*
			  LOAD_CONST(r[tT1], 16, cMV0Row0, 1) ; 
			  m4x4(r, tEyePos, pa, vPosition, r, tT1) ;
			*/

			/* Get temp for eye position */
			FFGenReg *psEyePosition = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 4, IMG_NULL);
		
			NEW_BLOCK(psFFGenCode, "Calculate Eye position");

			/* Get modelview matrix */
			psMatrix = GetReg(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_MODELVIEWMATRIX, 0, FFTNL_SIZE_MATRIX_4X4, "MV Matrix");

			/* Transform by position by MPV */
			M4x4(psFFGenCode, psEyePosition, psInputPosition, psMatrix, 0, "Calculate eye position");

			/* Release MV constant */
			ReleaseReg(psFFGenCode, psMatrix);

			/* Store eye position for possible later use */
			psFFGenCode->psEyePosition = psEyePosition;
		}

		STRIP_ENABLE1(FFTNL_ENABLES1_EYEPOSITION);
	}
	else if (psFFGenCode->psEyePosition)
	{
		ReleaseReg(psFFGenCode, psFFGenCode->psEyePosition);
		psFFGenCode->psEyePosition = IMG_NULL;
	}

#if defined(DEBUG)
	/* Check for illegal setup of enables */
	if (ui32FFTNLEnables1)
	{
		psFFGenCode->psFFGenContext->pfnPrint("Illegal setup of transformation enables? (%08X)\n", ui32FFTNLEnables1);
	}
#endif
	
}

/******************************************************************************
 * Function Name: FFTNLGenClipping
 * Inputs       : psFFGenCode
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
static IMG_VOID FFTNLGenClipping(FFGenCode *psFFGenCode)
{
	/* Get the FF description */
	FFTNLGenDesc *psFFTNLGenDesc = psFFGenCode->psFFTNLGenDesc;
	
	/* Get the enables */
	IMG_UINT32 ui32FFTNLEnables1 = psFFTNLGenDesc->ui32FFTNLEnables1;

	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

	if (ui32FFTNLEnables1 & FFTNL_ENABLES1_CLIPPING)
	{
		/*
		  fdp4			o[il],		r[tEyePos],				r[tT1] ;
		*/

		IMG_UINT32 uEnabledClipPlanes = psFFTNLGenDesc->uEnabledClipPlanes;
		
		/* quick sanity check */
		if (!uEnabledClipPlanes)
		{
			psFFGenCode->psFFGenContext->pfnPrint("Illegal setup of clipping state\n");
			return;
		}
		
		NEW_BLOCK(psFFGenCode, "User Clipping");
		
		{
			IMG_UINT32 uOutputCount = 0;
	
			IMG_UINT32 uConstantCount = 0;
	
			FFGenReg *psOutputClip, *psClipPlanes;
	
			FFGenReg *psPosition = psFFGenCode->psEyePosition;
	
			/* get output reg for clipping results */
			psOutputClip = GetReg(psFFGenCode,
								  USEASM_REGTYPE_OUTPUT,
								  FFGEN_OUTPUT_CLIPVERTEX,
								  0,
								  psFFGenCode->uNumOutputClipPlanes,
								  IMG_NULL);
	
			/* get clip planes */
			psClipPlanes =  GetReg(psFFGenCode,
								   USEASM_REGTYPE_SECATTR,
								   FFGEN_STATE_CLIPPLANE,
								   0,
								   psFFGenCode->uNumConstantClipPlanes * FFTNL_SIZE_CLIPPLANE,
								   "Clip planes");
	
			/* Process all the clip planes */
			while (uEnabledClipPlanes)
			{
				if (uEnabledClipPlanes & 0x1)
				{
					/* Clip plane enables are packed so we must pack the clip plane regs too */
					SETDESTOFF(DP4_ADJUST(uOutputCount)); SETSRCOFF(0, uOutputCount);  SETSRCOFF(2, uConstantCount * FFTNL_SIZE_CLIPPLANE);
					INST3(FDPC4, psOutputClip, &(psFFGenCode->sClipPlaneReg), psPosition, psClipPlanes, "Clip plane calc");
	
					/* Increase output clip plane count */
					uOutputCount++;
				}
	
				/* Increase constant clip plane count */
				uConstantCount++;
	
				uEnabledClipPlanes >>= 1;
			}
	
			ReleaseReg(psFFGenCode, psClipPlanes);
			
		}
	}
}

/******************************************************************************
 * Function Name: FFTNLGenColours
 * Inputs       : psFFGenCode
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
static IMG_VOID FFTNLGenColours(FFGenCode *psFFGenCode)
{
	/* Get the FF description */
	FFTNLGenDesc *psFFTNLGenDesc = psFFGenCode->psFFTNLGenDesc;

	/* Get the enables */
	IMG_UINT32 ui32FFTNLEnables1 = psFFTNLGenDesc->ui32FFTNLEnables1;

	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

	/* Pass the colours through if lighting not enabled */
	if (!(ui32FFTNLEnables1 & FFTNL_ENABLES1_LIGHTINGENABLED))
	{
#if defined(FIX_HW_BRN_25211)
		if(psFFTNLGenDesc->ui32FFTNLEnables2 & FFTNL_ENABLES2_CLAMP_OUTPUT_COLOURS)
		{
			FFGenReg *psInputColour = GetReg(psFFGenCode, USEASM_REGTYPE_PRIMATTR, FFGEN_INPUT_COLOR, 0, 4, IMG_NULL);
			FFGenReg *psOutputColour = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTCOLOR, 0, 4, IMG_NULL);
			FFGenReg *psImmediateFloatReg  = &(psFFGenCode->sImmediateFloatReg);


			/* Clamp to 0 */
			SET_FLOAT_ZERO(psImmediateFloatReg);

			SETDESTOFF(0); SETSRCOFF(0, 0);
			INST2(FMAX, psOutputColour, psInputColour, psImmediateFloatReg, "Clamp colour channel 0 to 0.0");

			SETDESTOFF(1); SETSRCOFF(0, 1);
			INST2(FMAX, psOutputColour, psInputColour, psImmediateFloatReg, "Clamp colour channel 1 to 0.0");

			SETDESTOFF(2); SETSRCOFF(0, 2);
			INST2(FMAX, psOutputColour, psInputColour, psImmediateFloatReg, "Clamp colour channel 2 to 0.0");

			SETDESTOFF(3); SETSRCOFF(0, 3);
			INST2(FMAX, psOutputColour, psInputColour, psImmediateFloatReg, "Clamp colour channel 3 to 0.0");

			/* Clamp to 1 */
			SET_FLOAT_ONE(psImmediateFloatReg);

			SETDESTOFF(0); SETSRCOFF(0, 0);
			INST2(FMIN, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 0 to 1.0");

			SETDESTOFF(1); SETSRCOFF(0, 1);
			INST2(FMIN, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 1 to 1.0");

			SETDESTOFF(2); SETSRCOFF(0, 2);
			INST2(FMIN, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 2 to 1.0");

			SETDESTOFF(3); SETSRCOFF(0, 3);
			INST2(FMIN, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 3 to 1.0");


			if (ui32FFTNLEnables1 & FFTNL_ENABLES1_SECONDARYCOLOUR)
			{
				FFGenReg *psInputColour = GetReg(psFFGenCode, USEASM_REGTYPE_PRIMATTR, FFGEN_INPUT_SECONDARYCOLOR, 0, 4, IMG_NULL);
				FFGenReg *psOutputColour = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTSECONDARYCOLOR, 0, 4, IMG_NULL);

				/* Clamp to 0 */
				SET_FLOAT_ZERO(psImmediateFloatReg);

				SETDESTOFF(0); SETSRCOFF(0, 0);
				INST2(FMAX, psOutputColour, psInputColour, psImmediateFloatReg, "Clamp colour channel 0 to 0.0");

				SETDESTOFF(1); SETSRCOFF(0, 1);
				INST2(FMAX, psOutputColour, psInputColour, psImmediateFloatReg, "Clamp colour channel 1 to 0.0");

				SETDESTOFF(2); SETSRCOFF(0, 2);
				INST2(FMAX, psOutputColour, psInputColour, psImmediateFloatReg, "Clamp colour channel 2 to 0.0");

				SETDESTOFF(3); SETSRCOFF(0, 3);
				INST2(FMAX, psOutputColour, psInputColour, psImmediateFloatReg, "Clamp colour channel 3 to 0.0");


				/* Clamp to 1 */
				SET_FLOAT_ONE(psImmediateFloatReg);

				SETDESTOFF(0); SETSRCOFF(0, 0);
				INST2(FMIN, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 0 to 1.0");

				SETDESTOFF(1); SETSRCOFF(0, 1);
				INST2(FMIN, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 1 to 1.0");

				SETDESTOFF(2); SETSRCOFF(0, 2);
				INST2(FMIN, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 2 to 1.0");

				SETDESTOFF(3); SETSRCOFF(0, 3);
				INST2(FMIN, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 3 to 1.0");
			}
		}
		else
#endif /* defined(FIX_HW_BRN_25211) */
		{
			/*
			  mov.rpt4		o[oPrimCol],			pa[vPrimCol];
			*/

			/* Get input colour reg */
			FFGenReg *psInputColour = GetReg(psFFGenCode, USEASM_REGTYPE_PRIMATTR, FFGEN_INPUT_COLOR, 0, 4, IMG_NULL);

			/* get output colour reg */
			FFGenReg *psOutputColour = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTCOLOR, 0, 4, IMG_NULL);

			NEW_BLOCK(psFFGenCode, "Colours");

			SET_REPEAT_COUNT(4);
			INST1(MOV, psOutputColour, psInputColour, "Pass through the colour");

			if (ui32FFTNLEnables1 & FFTNL_ENABLES1_SECONDARYCOLOUR)
			{
				/*
				  mov.rpt4		o[oSecCol],				pa[vSecCol];
				*/
				/* Get input colour reg */
				FFGenReg *psInputColour = GetReg(psFFGenCode, USEASM_REGTYPE_PRIMATTR, FFGEN_INPUT_SECONDARYCOLOR, 0, 4, IMG_NULL);
				
				/* get output colour reg */
				FFGenReg *psOutputColour = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTSECONDARYCOLOR, 0, 4, IMG_NULL);

				SET_REPEAT_COUNT(4);
				INST1(MOV, psOutputColour, psInputColour, "Pass through the secondary colour");
			}
		}
	}
}

/******************************************************************************
 * Function Name: FFTNLGenFog
 * Inputs       : psFFGenCode
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
static IMG_VOID FFTNLGenFog(FFGenCode *psFFGenCode)
{
	/* Get the FF description */
	FFTNLGenDesc *psFFTNLGenDesc = psFFGenCode->psFFTNLGenDesc;
	
	/* Get the fog enables */
	IMG_UINT32 ui32FFTNLEnables1 = psFFTNLGenDesc->ui32FFTNLEnables1 & (FFTNL_ENABLES1_FOGCOORD       |
																		 FFTNL_ENABLES1_FOGCOORDEYEPOS|
																		 FFTNL_ENABLES1_FOGLINEAR     |
																		 FFTNL_ENABLES1_FOGEXP        |
																		 FFTNL_ENABLES1_FOGEXP2		  |
																		 FFTNL_ENABLES1_RANGEFOG);
	IMG_UINT32 ui32FFTNLEnables2 = psFFTNLGenDesc->ui32FFTNLEnables2;

	if (ui32FFTNLEnables1)
	{
		FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

		FFGenReg *psFogCoord, *psFogParams = IMG_NULL;

		IMG_UINT32 uFogCoordFlag;

		IMG_UINT32 uFogCoordOffset;

		/* Get output fog register */
		FFGenReg *psOutputFogCoord = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FOGFRAGCOORD, 0, FOGDIM, IMG_NULL);

		NEW_BLOCK(psFFGenCode, "Fog");

		/* Load fog constant if required */
		if (ui32FFTNLEnables1 & (FFTNL_ENABLES1_FOGLINEAR |
							 FFTNL_ENABLES1_FOGEXP    |
							 FFTNL_ENABLES1_FOGEXP2))
		{
			/* Get the fog params */
			psFogParams = GetReg(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_PMXFOGPARAM, 0, 4, "Fog Params");
		}

		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_FOGCOORD)
		{
			/* Get fog coord from input register */
			psFogCoord = GetReg(psFFGenCode, USEASM_REGTYPE_PRIMATTR, FFGEN_INPUT_FOGCOORD, 0, 1, IMG_NULL);

			uFogCoordFlag   = 0;
			uFogCoordOffset = 0;

			STRIP_ENABLE1(FFTNL_ENABLES1_FOGCOORD);
		}
		else if (ui32FFTNLEnables1 & FFTNL_ENABLES1_FOGCOORDEYEPOS)
		{
			/* Use eye position for fog coord */
			psFogCoord = psFFGenCode->psEyePosition;
			
			/* Make sure it is +ve */
			uFogCoordFlag   = USEASM_ARGFLAGS_ABSOLUTE;
			uFogCoordOffset = 2;
			
			if (ui32FFTNLEnables1 & FFTNL_ENABLES1_RANGEFOG)
			{
				FFGenReg *psFogTemp = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);
				
				/*
				 fmsa		r[TEMP_FOG], r[TEMP_POSITION + 0], r[TEMP_POSITION + 1], r[TEMP_POSITION + 1]
				 fmad		r[TEMP_FOG], r[TEMP_POSITION + 2], r[TEMP_POSITION + 2], r[TEMP_FOG]
				 frsq		r[TEMP_FOG], r[TEMP_FOG]
				 frcp		r[TEMP_FOG], r[TEMP_FOG]
				*/
				SETSRCOFF(0, 0); SETSRCOFF(1,0);
				INST2(FMUL, psFogTemp, psFogCoord, psFogCoord, "Calculate range fog distance...");
				
				SETSRCOFF(0, 1); SETSRCOFF(1,1); SETSRCOFF(2,0);
				INST3(FMAD, psFogTemp, psFogCoord, psFogCoord, psFogTemp, IMG_NULL);
				
				SETSRCOFF(0, 2); SETSRCOFF(1,2); SETSRCOFF(2,0);
				INST3(FMAD, psFogTemp, psFogCoord, psFogCoord, psFogTemp, IMG_NULL);
				
				SETSRCOFF(0, 0); SETSRCOFF(1,0); SETSRCOFF(2,0);
				INST1(FRSQ, psFogTemp, psFogTemp, IMG_NULL);
				INST1(FRCP, psFogTemp, psFogTemp, IMG_NULL);
				
				// Use the result as input to the rest of the fog calculations..
				psFogCoord = psFogTemp;
				uFogCoordOffset = 0;
			}
			
			STRIP_ENABLE1(FFTNL_ENABLES1_FOGCOORDEYEPOS);
		}
		else if (ui32FFTNLEnables2 & FFTNL_ENABLES2_FOGCOORDZERO)
		{
			psFogCoord = &psFFGenCode->sImmediateFloatReg;

			SET_FLOAT_ZERO(psFogCoord);

			uFogCoordFlag   = 0;
			uFogCoordOffset = 0;

			STRIP_ENABLE2(FFTNL_ENABLES2_FOGCOORDZERO);
		}
		else
		{
			psFFGenCode->psFFGenContext->pfnPrint("FFTNLGenFog: FOGCOORD or FOGCOORDEYEPOS must be set if fog is enabled\n");
			COMMENT(psFFGenCode, "FFTNLGenFog: FOGCOORD or FOGCOORDEYEPOS must be set if fog is enabled\n");
			return;
		}

		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_FOGLINEAR)
		{
			/*
			  fmad			o[oFog],				r[tT3],			r[tT2 + _Z_],			r[tT2 + _W_];
			*/

			/* Add in abs and src offset if required */
			SETSRCFLAG(0, uFogCoordFlag);
			SETSRCOFF(0, uFogCoordOffset); SETSRCOFF(1,2); SETSRCOFF(2,3); 
			INST3(FMAD,
				  psOutputFogCoord,
				  psFogCoord,
				  psFogParams,
				  psFogParams,
				  "Linear Fog - Multiple fog coord by (-1.0f / (end - start)) and add to (end / (end - start))");

			ReleaseReg(psFFGenCode, psFogParams);

			STRIP_ENABLE1(FFTNL_ENABLES1_FOGLINEAR);
		}
		else if (ui32FFTNLEnables1 & FFTNL_ENABLES1_FOGEXP)
		{
			/*
			  fmul			r[tT1],					r[tT3],			r[tT2 + _X_] ;
			  fexp			o[oFog],				-r[tT1] ;
			*/

			FFGenReg *psFogTemp = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

			SETSRCFLAG(0, uFogCoordFlag);
			SETSRCOFF(0, uFogCoordOffset); SETSRCOFF(1,0);
			INST2(FMUL, psFogTemp, psFogCoord, psFogParams, "FOG EXP - Multiply eye distance by (adjusted) density");

			SETSRCFLAG(0, USEASM_ARGFLAGS_NEGATE); 
			INST1(FEXP, psOutputFogCoord, psFogTemp, "Get exponential value");

			ReleaseReg(psFFGenCode, psFogTemp);
			ReleaseReg(psFFGenCode, psFogParams);

			STRIP_ENABLE1(FFTNL_ENABLES1_FOGEXP);
		}
		else if (ui32FFTNLEnables1 & FFTNL_ENABLES1_FOGEXP2)
		{
			/*
			  fmul			r[tT1],					r[tT3],			r[tT2 + _Y_] ;
			  fmul			r[tT1],				    r[tT1],         r[tT1] ;
			  fexp			o[oFog],			   -r[tT1] ;
			*/

			FFGenReg *psFogTemp = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

			SETSRCFLAG(0, uFogCoordFlag);
			SETSRCOFF(0, uFogCoordOffset); SETSRCOFF(1,1);
			INST2(FMUL, psFogTemp, psFogCoord, psFogParams, "FOG EXP2 - Multiply eye distance by (adjusted) density");

			INST2(FMUL, psFogTemp, psFogTemp, psFogTemp, "Square it");

			SETSRCFLAG(0, USEASM_ARGFLAGS_NEGATE); 
			INST1(FEXP, psOutputFogCoord, psFogTemp, "Get exponential value");

			ReleaseReg(psFFGenCode, psFogTemp);
			ReleaseReg(psFFGenCode, psFogParams);

			STRIP_ENABLE1(FFTNL_ENABLES1_FOGEXP2);
		}
		else
		{
			/*
			  mov				o[oFog],			r[tEyePos + _Z_].abs / pa[vFogCoord];
			*/

			/* Add in abs if required */
			SETSRCFLAG(0, uFogCoordFlag);

			SETSRCOFF(0, uFogCoordOffset);
			INST1(FMOV, psOutputFogCoord, psFogCoord, "Pass through the fog coord");
		}
		
		if(ui32FFTNLEnables1 & FFTNL_ENABLES1_RANGEFOG)
		{
			ReleaseReg(psFFGenCode, psFogCoord);
			
			STRIP_ENABLE1(FFTNL_ENABLES1_RANGEFOG);
		}
	}

#if defined(DEBUG)

	/* Check for illegal setup of enables */
	if (ui32FFTNLEnables1)
	{
		psFFGenCode->psFFGenContext->pfnPrint("Illegal setup of fog enables? (%08X)\n", ui32FFTNLEnables1);
	}

#endif

}

/******************************************************************************
 * Function Name: FFTNLGenVertexToEyeVector
 * Inputs       : psFFGenCode
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
static IMG_VOID FFTNLGenVertexToEyeVector(FFGenCode *psFFGenCode)
{
	/* Get the FF description */
	FFTNLGenDesc *psFFTNLGenDesc = psFFGenCode->psFFTNLGenDesc;
	
	/* Get the enables */
	IMG_UINT32 ui32FFTNLEnables1 = psFFTNLGenDesc->ui32FFTNLEnables1;

	if (ui32FFTNLEnables1 & FFTNL_ENABLES1_VERTEXTOEYEVECTOR)
	{
		/*
		  fdp3			r[DP3_DST(tT1)],		r[tEyePos],			r[tEyePos];
		  frsq			r[tT2],					r[tT1];
		  fmul			r[tVtxEyeVec +_X_],		r[tEyePos +_X_],	-r[tT2];
		  fmul			r[tVtxEyeVec +_Y_],		r[tEyePos +_Y_],	-r[tT2];
		  fmul			r[tVtxEyeVec +_Z_],		r[tEyePos +_Z_],	-r[tT2];
		*/

		FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

		FFGenReg *psVtxEyeVecMag    = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);
		FFGenReg *psRSQVtxEyeVecMag = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);
		FFGenReg *psVtxEyeVec       = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);
		FFGenReg *psEyePosition     = psFFGenCode->psEyePosition;

		NEW_BLOCK(psFFGenCode, "Vertex to Eye Vector");

		COMMENT(psFFGenCode, "Calculate eye to vertex vector up front as it can be used by multiple");
		COMMENT(psFFGenCode, "lights and the texture generation code and point attenutation");

		SETDESTOFF(DP3_ADJUST(0));
		INST2(FDP3, psVtxEyeVecMag, psEyePosition , psEyePosition , "Calculate normalised eye vector");
		INST1(FRSQ, psRSQVtxEyeVecMag, psVtxEyeVecMag, IMG_NULL);
		
		SETDESTOFF(0);  SETSRCOFF(0, 0); 
		SETSRCFLAG(1, USEASM_ARGFLAGS_NEGATE);
		INST2(FMUL, psVtxEyeVec, psEyePosition , psRSQVtxEyeVecMag, IMG_NULL);

		SETDESTOFF(1);  SETSRCOFF(0, 1); 
		SETSRCFLAG(1, USEASM_ARGFLAGS_NEGATE);
		INST2(FMUL, psVtxEyeVec, psEyePosition , psRSQVtxEyeVecMag, IMG_NULL);
		
		SETDESTOFF(2);  SETSRCOFF(0, 2); 
		SETSRCFLAG(1, USEASM_ARGFLAGS_NEGATE);
		INST2(FMUL, psVtxEyeVec, psEyePosition , psRSQVtxEyeVecMag, IMG_NULL);

		/* Can use some of the value calculated here later on when doing point attentuation */
		if ( ui32FFTNLEnables1 & FFTNL_ENABLES1_POINTATTEN)
		{
			psFFGenCode->psVtxEyeVecMag    = psVtxEyeVecMag;
			psFFGenCode->psRSQVtxEyeVecMag = psRSQVtxEyeVecMag;
		}
		else
		{
			ReleaseReg(psFFGenCode, psVtxEyeVecMag);
			ReleaseReg(psFFGenCode, psRSQVtxEyeVecMag);
		}
		/* Store this for later use */
		psFFGenCode->psVtxEyeVec = psVtxEyeVec;
	}
}


/******************************************************************************
 * Function Name: FFTNLGenPoints
 * Inputs       : psFFGenCode
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
static IMG_VOID FFTNLGenPoints(FFGenCode *psFFGenCode)
{
	/* Get the FF description */
	FFTNLGenDesc *psFFTNLGenDesc = psFFGenCode->psFFTNLGenDesc;

	/* Get the enables */
	IMG_UINT32 ui32FFTNLEnables1 = psFFTNLGenDesc->ui32FFTNLEnables1;
	IMG_UINT32 ui32FFTNLEnables2 = psFFTNLGenDesc->ui32FFTNLEnables2;

	if (ui32FFTNLEnables2 & (FFTNL_ENABLES2_POINTS|FFTNL_ENABLES2_POINTSPRITES))
	{
		FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

		FFGenReg *psOutputPointSize;

		NEW_BLOCK(psFFGenCode, "Points");

		/* get output point size reg */
		psOutputPointSize = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_POINTSIZE, 0, 1, IMG_NULL);

		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_POINTATTEN)
		{
			/* Reuse values from vertex eye vector calculations */
			FFGenReg *psVtxEyeVecMag      = psFFGenCode->psVtxEyeVecMag;
			FFGenReg *psRSQVtxEyeVecMag   = psFFGenCode->psRSQVtxEyeVecMag;

			FFGenReg *psImmediateFloatReg = &(psFFGenCode->sImmediateFloatReg);

			FFGenReg *psInputPointSize = IMG_NULL;

			/* Get predicate regs */
			FFGenReg *psPredReg0        = &(psFFGenCode->asPredicateRegs[0]);
			FFGenReg *psPredReg1        = &(psFFGenCode->asPredicateRegs[1]);

			/* Get point size value */
			FFGenReg *psPointParams     =  GetReg(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_POINT, 0, FFTNL_SIZE_POINTPARAMS, "Point params");

			/* Get temp */
			FFGenReg *psDistanceAtten   = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);
					
			if (ui32FFTNLEnables1 & FFTNL_ENABLES1_POINTSIZEARRAY)
			{
				/* Get input point size */
				psInputPointSize     =  GetReg(psFFGenCode, USEASM_REGTYPE_PRIMATTR, FFGEN_INPUT_POINTSIZE, 0, 1, "Point size");				
			}
				
			/*
			  mov		r[tDistAtten + _X_],	#1.0
			  fmul		r[tDistAtten + _Y_],	r[tT1],				r[tT2] ;
			  mov		r[tDistAtten + _Z_],	r[tT1] ;
			*/

			SETDESTOFF(0);   
			SET_FLOAT_ONE(psImmediateFloatReg);
			INST1(MOV, psDistanceAtten, psImmediateFloatReg, " tDistAtten = 1.0, |Pe|, |Pe|^2, 1/|Pe|");

			SETDESTOFF(1);   
			INST2(FMUL, psDistanceAtten, psVtxEyeVecMag, psRSQVtxEyeVecMag, IMG_NULL);

			SETDESTOFF(2);   
			INST1(MOV, psDistanceAtten,  psVtxEyeVecMag, IMG_NULL);

			/*
			  fdp3			r[DP3_DST(tDistAtten)],	r[tDistAtten],		r[tT3] ; 
			  frcp			r[tDistAtten],			r[tDistAtten] ;
			*/
			SETDESTOFF(DP3_ADJUST(0)); SETSRCOFF(1, FFTNL_OFFSETS_POINTPARAMS_CONATTEN);
			INST2(FDP3, psDistanceAtten, psDistanceAtten, psPointParams, "Calculate attenuation");
			INST1(FRSQ, psDistanceAtten, psDistanceAtten, IMG_NULL);

			if (ui32FFTNLEnables1 & FFTNL_ENABLES1_POINTSIZEARRAY)
			{
				/*
				  fmul			o[oPSize],				r[tDistAtten],		pa[iPointSize] ; 
				*/
				SETSRCOFF(1, FFTNL_OFFSETS_POINTPARAMS_SIZE);
				INST2(FMUL, psOutputPointSize, psDistanceAtten, psInputPointSize, "Multiply attenuation by the point size");
			}
			else
			{
				/*
				  fmul			o[oPSize],				r[tDistAtten],		r[tT4 + _X_] ; 
				*/
				SETSRCOFF(1, FFTNL_OFFSETS_POINTPARAMS_SIZE);
				INST2(FMUL, psOutputPointSize, psDistanceAtten, psPointParams, "Multiply attenuation by requested point size");
			}

			/*
			  fsub.testn		p0,						o[oPSize],			r[tT4 + _Y_] ; 
			  fsub.testp		p1,						o[oPSize],			r[tT4 + _Z_] ; 
			*/
			SET_TESTPRED_NEGATIVE();
			SETSRCOFF(1, FFTNL_OFFSETS_POINTPARAMS_MIN);
			INST2(FSUB, psPredReg0, psOutputPointSize, psPointParams, "Test if size is within min and max");

			SET_TESTPRED_POSITIVE();
			SETSRCOFF(1, FFTNL_OFFSETS_POINTPARAMS_MAX);
			INST2(FSUB, psPredReg1, psOutputPointSize, psPointParams, IMG_NULL);


			/*
			  p0	mov				o[oPSize],				r[tT4 + _Y_] ;
			  p1	mov				o[oPSize],				r[tT4 + _Z_] ;
			*/

			USE_PRED(psPredReg0);
			SETSRCOFF(0, FFTNL_OFFSETS_POINTPARAMS_MIN);
			INST1(MOV, psOutputPointSize, psPointParams, "Clamp the result if required");

			USE_PRED(psPredReg1);
			SETSRCOFF(0, FFTNL_OFFSETS_POINTPARAMS_MAX);
			INST1(MOV, psOutputPointSize, psPointParams, IMG_NULL);

#if defined(FFGEN_UNIFLEX)
			/*
			 * Round to nearest integer - add 0.5 then floor
			 */
			SET_FLOAT_HALF(psImmediateFloatReg);
			INST2(FADD, psOutputPointSize, psOutputPointSize, psImmediateFloatReg, "Add 0.5f");

			INST1(FLOOR, psOutputPointSize, psOutputPointSize, "floor(pointsize)");
#endif /* defined(FFGEN_UNIFLEX) */

			if (ui32FFTNLEnables1 & FFTNL_ENABLES1_POINTSIZEARRAY)
			{
				ReleaseReg(psFFGenCode, psInputPointSize);
			}

			ReleaseReg(psFFGenCode, psDistanceAtten);
			ReleaseReg(psFFGenCode, psPointParams);

			ReleaseReg(psFFGenCode, psVtxEyeVecMag);
			ReleaseReg(psFFGenCode, psRSQVtxEyeVecMag);

			psFFGenCode->psVtxEyeVecMag = IMG_NULL;
			psFFGenCode->psRSQVtxEyeVecMag = IMG_NULL;
		}
		else if(ui32FFTNLEnables1 & FFTNL_ENABLES1_POINTSIZEARRAY)
		{
			/* Get predicate regs */
			FFGenReg *psPredReg0        = &(psFFGenCode->asPredicateRegs[0]);
			FFGenReg *psPredReg1        = &(psFFGenCode->asPredicateRegs[1]);
			FFGenReg *psPointParams, *psInputPointSize;
#if defined(FFGEN_UNIFLEX)
			FFGenReg *psImmediateFloatReg = &(psFFGenCode->sImmediateFloatReg);
#endif /* defined(FFGEN_UNIFLEX) */

			/* Get input point size */
			psInputPointSize     =  GetReg(psFFGenCode, USEASM_REGTYPE_PRIMATTR, FFGEN_INPUT_POINTSIZE, 0, 1, "Point size");				
			
			/*
				mov				o[oPSize],				pa[iPointSize] ; 
			*/
			INST1(MOV, psOutputPointSize, psInputPointSize, "Pass through the point size");

			ReleaseReg(psFFGenCode, psInputPointSize);
			
			psPointParams     =  GetReg(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_POINT, 0, FFTNL_SIZE_POINTPARAMS, "Point params");
			
			/*
			  fsub.testn		p0,						o[oPSize],			r[tT4 + _Y_] ; 
			  fsub.testp		p1,						o[oPSize],			r[tT4 + _Z_] ; 
			*/
			SET_TESTPRED_NEGATIVE();
			SETSRCOFF(1, FFTNL_OFFSETS_POINTPARAMS_MIN);
			INST2(FSUB, psPredReg0, psOutputPointSize, psPointParams, "Test if size is within min and max");

			SET_TESTPRED_POSITIVE();
			SETSRCOFF(1, FFTNL_OFFSETS_POINTPARAMS_MAX);
			INST2(FSUB, psPredReg1, psOutputPointSize, psPointParams, IMG_NULL);

			/*
			  p0	mov				o[oPSize],				r[tT4 + _Y_] ;
			  p1	mov				o[oPSize],				r[tT4 + _Z_] ;
			*/

			USE_PRED(psPredReg0);
			SETSRCOFF(0, FFTNL_OFFSETS_POINTPARAMS_MIN);
			INST1(MOV, psOutputPointSize, psPointParams, "Clamp the result if required");

			USE_PRED(psPredReg1);
			SETSRCOFF(0, FFTNL_OFFSETS_POINTPARAMS_MAX);
			INST1(MOV, psOutputPointSize, psPointParams, IMG_NULL);
		
#if defined(FFGEN_UNIFLEX)
			/*
			 * Round to nearest integer - add 0.5 then floor
			 */
			SET_FLOAT_HALF(psImmediateFloatReg);
			INST2(FADD, psOutputPointSize, psOutputPointSize, psImmediateFloatReg, "Add 0.5f");

			INST1(FLOOR, psOutputPointSize, psOutputPointSize, "floor(pointsize)");
#endif /* defined(FFGEN_UNIFLEX) */

			ReleaseReg(psFFGenCode, psPointParams);
		}
		else
		{
			/*
			  LOAD_CONST(r[tT1], 1, cPtSize, 0) ; 
		  
			  mov				o[oPSize],				r[tT1] ; 
			*/

			/* Get point size value */
			FFGenReg *psPointParams = GetReg(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_PMXPOINTSIZE, 0, 1, "Point size");
			
			SETSRCOFF(0, FFTNL_OFFSETS_POINTPARAMS_SIZE);
			INST1(MOV, psOutputPointSize, psPointParams, "Pass through the point size");

			ReleaseReg(psFFGenCode, psPointParams);
		}
	}
}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
static IMG_VOID FFTNLGenLinearTexGenCode(FFGenCode  *psFFGenCode,
								  IMG_UINT32     uUnitCount,
								  IMG_UINT8     *pubLinearTexGenCoordMasks,
								  FFGenReg      *psOutputTexCoords,
								  FFGenReg      *psInputRegister,
								  FFGenRegDesc   eBindingRegDesc,
								  IMG_CHAR      *pszDesc)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

	IMG_UINT8 ubLinearCoordMask = pubLinearTexGenCoordMasks[uUnitCount];

	IMG_UINT32 uCoordCount = 0;

	COMMENT(psFFGenCode, "Generate %s texture coordinates", pszDesc);

	while(ubLinearCoordMask)
	{
		if (ubLinearCoordMask & 0x1)
		{
			/* Get the tex gen eye plane */
			FFGenReg *psPlane = GetReg(psFFGenCode,
									   USEASM_REGTYPE_SECATTR,
									   eBindingRegDesc + uCoordCount,
									   uUnitCount * FFTNL_SIZE_TEXGENPLANE,
									   FFTNL_SIZE_TEXGENPLANE,
									   pszDesc);

			SETDESTOFF(DP4_ADJUST(uCoordCount));
			INST2(FDP4, &(psOutputTexCoords[uUnitCount]), psInputRegister, psPlane, IMG_NULL);

			ReleaseReg(psFFGenCode, psPlane);
		}

		/* Shift mask */
		ubLinearCoordMask >>= 1;

		uCoordCount++;
	}
}


/******************************************************************************
 * Function Name: FFTNLGenTexPassthroughCode
 * Inputs       : 
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
static IMG_VOID FFTNLGenTexPassthroughCode(FFGenCode  *psFFGenCode,
									IMG_UINT8      ubCoordMask,
									FFGenReg      *psOutputReg,
									FFGenReg      *psInputReg,
									IMG_CHAR      *pszDesc)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

	IMG_UINT32 uCopySize;

	/* Check for contiguous copies */
	switch (ubCoordMask)
	{
		case 0x0:
			psFFGenCode->psFFGenContext->pfnPrint("FFTNLGenTexturing: %s texture enabled but no coordinates\n", pszDesc);
			uCopySize = 0;
			break;
		case 0x1:
			uCopySize = 1;
			break;
		case 0x3:
			uCopySize = 2;
			break;
		case 0x7:
			uCopySize = 3;
			break;
		case 0xF:
			uCopySize = 4;
			break;
		default:
			uCopySize = 0;
			break;
	}

	COMMENT(psFFGenCode, "Gen %s texture coordinates", pszDesc); 

	/* Can we do a contiguous copy? */
	if (uCopySize)
	{
		SET_REPEAT_COUNT(uCopySize);
		INST1(MOV, psOutputReg, psInputReg, IMG_NULL);
	}
	/* Nope - copy each coord individually */
	else
	{
		IMG_UINT32 uCoordCount = 0;

		while (ubCoordMask)
		{
			if (ubCoordMask & 0x1)
			{
				SETDESTOFF(uCoordCount); SETSRCOFF(0, uCoordCount);
				INST1(MOV, psOutputReg, psInputReg, IMG_NULL);
			}

			/* Shift mask */
			ubCoordMask >>= 1;

			uCoordCount++;
		}
	}
}


/******************************************************************************
 * Function Name: FFTNLGenTexturing
 * Inputs       : psFFGenCode
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
static IMG_VOID FFTNLGenTexturing(FFGenCode *psFFGenCode)
{
	/* Get the FF description */
	FFTNLGenDesc *psFFTNLGenDesc = psFFGenCode->psFFTNLGenDesc;
	
	/* Get the texturing enables */
	IMG_UINT32 ui32FFTNLEnables1 = psFFTNLGenDesc->ui32FFTNLEnables1 & (FFTNL_ENABLES1_TEXTURINGENABLED |
																	 FFTNL_ENABLES1_TEXGENSPHEREMAP  |
																	 FFTNL_ENABLES1_REFLECTIONMAP    |
																	 FFTNL_ENABLES1_TEXGEN);
	
	IMG_UINT32 ui32FFTNLEnables2 = psFFTNLGenDesc->ui32FFTNLEnables2 & (FFTNL_ENABLES2_TEXCOORD_INDICES);
	
	IMG_UINT32 uEnabledPassthroughCoords   = psFFTNLGenDesc->uEnabledPassthroughCoords;
	IMG_UINT32 uEnabledEyeLinearCoords     = psFFTNLGenDesc->uEnabledEyeLinearCoords;
	IMG_UINT32 uEnabledObjLinearCoords     = psFFTNLGenDesc->uEnabledObjLinearCoords;
	IMG_UINT32 uEnabledSphereMapCoords     = psFFTNLGenDesc->uEnabledSphereMapCoords;
	IMG_UINT32 uEnabledNormalMapCoords     = psFFTNLGenDesc->uEnabledNormalMapCoords;
	IMG_UINT32 uEnabledPositionMapCoords   = psFFTNLGenDesc->uEnabledPositionMapCoords;
	IMG_UINT32 uEnabledReflectionMapCoords = psFFTNLGenDesc->uEnabledReflectionMapCoords;
	IMG_UINT32 uEnabledTextureMatrices     = psFFTNLGenDesc->uEnabledTextureMatrices;
	
	/* Get all enabled tex gen coords */
	IMG_UINT32 uEnabledTexGenCoords = (uEnabledEyeLinearCoords     |
									   uEnabledObjLinearCoords     |
									   uEnabledSphereMapCoords     |
									   uEnabledNormalMapCoords     |
									   uEnabledPositionMapCoords   |
									   uEnabledReflectionMapCoords);
	
	/* Get an amalgamantion of all enabled texture coords */
	IMG_UINT32 uEnabledCoords = (uEnabledTexGenCoords        |
								 uEnabledPassthroughCoords   |   
								 uEnabledTextureMatrices);
	
	if (ui32FFTNLEnables1 & FFTNL_ENABLES1_TEXTURINGENABLED)
	{
		FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

		IMG_UINT32 uUnitCount = 0, i;

		FFGenReg *psOutputTexCoords;
		FFGenReg  asOutputTexCoords[FFTNLGEN_MAX_NUM_TEXTURE_UNITS];
		FFGenReg *ppsInputTexCoords[FFTNLGEN_MAX_NUM_TEXTURE_UNITS];

		IMG_UINT32 auOutputTextCoordsOffsets[FFTNLGEN_MAX_NUM_TEXTURE_UNITS];
		IMG_UINT32 uOutputTexCoordSize = 0;

		/*
		   If only a texture matrix is being aplied we can bypass the passthrough code and apply the 
		   texture matrix transform directly to the input coords 
		*/
		IMG_UINT32 uTexMatrixUseInputTexCoordRegsMask = 0;

		NEW_BLOCK(psFFGenCode, "Texturing");


		/* Work out size of input and output regs required */
		while (uEnabledCoords)
		{
			/* Allocate input register if required */
			if (psFFGenCode->auInputTexDimensions[uUnitCount])
			{
				IMG_UINT32 uInputCoord = uUnitCount;
				
				if(ui32FFTNLEnables2 & FFTNL_ENABLES2_TEXCOORD_INDICES)
				{
					uInputCoord = psFFTNLGenDesc->aubPassthroughCoordIndex[uUnitCount];
				}
				
				ppsInputTexCoords[uUnitCount] = GetReg(psFFGenCode,
													   USEASM_REGTYPE_PRIMATTR,
													   FFGEN_INPUT_MULTITEXCOORD0 + uInputCoord,
													   0,
													   psFFGenCode->auInputTexDimensions[uUnitCount],
													   IMG_NULL);
			}
			else
			{
				ppsInputTexCoords[uUnitCount] = NULL;
			}

			/* Store offset to this output texture coordinate */
			auOutputTextCoordsOffsets[uUnitCount] = uOutputTexCoordSize;

			/* Increase offset */
			uOutputTexCoordSize += psFFGenCode->auOutputTexDimensions[uUnitCount];

			/* Are we only applying a texture matrix transform */
			if (!(uEnabledTexGenCoords    & (1 << uUnitCount)) &&
				(uEnabledTextureMatrices  & (1 << uUnitCount)) &&
				!(ui32FFTNLEnables2 & FFTNL_ENABLES2_TEXCOORD_INDICES))
			{
				uTexMatrixUseInputTexCoordRegsMask |= (1 << uUnitCount);
			}

			/* Shift mask */
			uEnabledCoords >>= 1;

			/* Increase unit count */
			uUnitCount++;
		}

		/* Create array for output texture coordinates */
		psOutputTexCoords = GetReg(psFFGenCode,
								   USEASM_REGTYPE_OUTPUT,
								   FFGEN_OUTPUT_TEXCOORD,
								   0,
								   uOutputTexCoordSize,
								   IMG_NULL);


		/* Create a temporary set of registers to describe output texture coordinates */
		for (i = 0; i < uUnitCount; i++)
		{
			/* Copy most of the info */
			asOutputTexCoords[i]     = *psOutputTexCoords;

			/* Set up info for this reg */
			asOutputTexCoords[i].uSizeInDWords  = psFFGenCode->auOutputTexDimensions[i];
			asOutputTexCoords[i].uOffset       += auOutputTextCoordsOffsets[i];
		}


		/* Reset unit count */
		uUnitCount = 0;

		if (uEnabledPassthroughCoords)
		{
			NEW_BLOCK(psFFGenCode, "Pass through texture coordinates");


			/* Deal with passthrough coords 1st */
			while (uEnabledPassthroughCoords)
			{
				/*
				   Only pass through if we're going to be doing tex gen or there's no matrix transform 
				   - If there's just a matrix transform we'll take the data directly from the input coord
				*/
				if (uEnabledPassthroughCoords & 0x1 && 	!(uTexMatrixUseInputTexCoordRegsMask & (1 << uUnitCount)))
				{
					FFTNLGenTexPassthroughCode(psFFGenCode,
											   psFFTNLGenDesc->aubPassthroughCoordMask[uUnitCount],
											   &(asOutputTexCoords[uUnitCount]),
											   ppsInputTexCoords[uUnitCount],
											   "Pass through");
				}

				/* Shift mask */
				uEnabledPassthroughCoords >>= 1;

				uUnitCount++;
			}

			/* Reset unit count */
			uUnitCount = 0;
		}

		if (uEnabledEyeLinearCoords)
		{
			IMG_UINT32 uNumPlanes[4] = {0, 0, 0, 0};
			IMG_UINT32 i;
			FFGenReg *psEyePosition = psFFGenCode->psEyePosition;

			/* Calculate number of planes required */
			while (uEnabledEyeLinearCoords)
			{
				for (i = 0; i < 4; i++)
				{
					if (psFFTNLGenDesc->aubEyeLinearCoordMask[uUnitCount] & (0x1 << i))
					{
						uNumPlanes[i] = uUnitCount + 1;
					}
				}

				/* Shift mask */
				uEnabledEyeLinearCoords >>= 1;

				uUnitCount++;
			}

			/* Allocate space for the eye linear planes */
			if (uNumPlanes[0]) AllocRegSpace(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_EYEPLANES, FFTNL_SIZE_TEXGENPLANE * uNumPlanes[0], IMG_FALSE);
			if (uNumPlanes[1]) AllocRegSpace(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_EYEPLANET, FFTNL_SIZE_TEXGENPLANE * uNumPlanes[1], IMG_FALSE);
			if (uNumPlanes[2]) AllocRegSpace(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_EYEPLANER, FFTNL_SIZE_TEXGENPLANE * uNumPlanes[2], IMG_FALSE);
			if (uNumPlanes[3]) AllocRegSpace(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_EYEPLANEQ, FFTNL_SIZE_TEXGENPLANE * uNumPlanes[3], IMG_FALSE);

			/* restore mask */
			uEnabledEyeLinearCoords     = psFFTNLGenDesc->uEnabledEyeLinearCoords;

			uUnitCount = 0;
			
			NEW_BLOCK(psFFGenCode, "Eye Linear tex gen");

			/* Eye linear tex gen */
			while (uEnabledEyeLinearCoords)
			{
				if (uEnabledEyeLinearCoords & 0x1)
				{
					FFTNLGenLinearTexGenCode(psFFGenCode,
											 uUnitCount,
											 psFFTNLGenDesc->aubEyeLinearCoordMask,
											 asOutputTexCoords,
											 psEyePosition,
											 FFGEN_STATE_EYEPLANES,
											 "Eye Linear");
				}

				/* Shift mask */
				uEnabledEyeLinearCoords >>= 1;

				uUnitCount++;
			}

			/* Reset unit count */
			uUnitCount = 0;
		}

		if (uEnabledObjLinearCoords)
		{
			IMG_UINT32 uNumPlanes[4] = {0, 0, 0, 0};
			IMG_UINT32 i;
			FFGenReg *psInputPosition = GetReg(psFFGenCode, USEASM_REGTYPE_PRIMATTR,  FFGEN_INPUT_VERTEX,   0, 4, IMG_NULL);

			/* Calculate number of planes required */
			while (uEnabledObjLinearCoords)
			{
				for (i = 0; i < 4; i++)
				{
					if (psFFTNLGenDesc->aubObjLinearCoordMask[uUnitCount] & (0x1 << i))
					{
						uNumPlanes[i] = uUnitCount + 1;
					}
				}

				/* Shift mask */
				uEnabledObjLinearCoords >>= 1;

				uUnitCount++;
			}

			/* Allocate space for the object linear planes */
			if (uNumPlanes[0]) AllocRegSpace(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_OBJECTPLANES, FFTNL_SIZE_TEXGENPLANE * uNumPlanes[0], IMG_FALSE);
			if (uNumPlanes[1]) AllocRegSpace(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_OBJECTPLANET, FFTNL_SIZE_TEXGENPLANE * uNumPlanes[1], IMG_FALSE);
			if (uNumPlanes[2]) AllocRegSpace(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_OBJECTPLANER, FFTNL_SIZE_TEXGENPLANE * uNumPlanes[2], IMG_FALSE);
			if (uNumPlanes[3]) AllocRegSpace(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_OBJECTPLANEQ, FFTNL_SIZE_TEXGENPLANE * uNumPlanes[3], IMG_FALSE);

			/* restore mask */
			uEnabledObjLinearCoords     = psFFTNLGenDesc->uEnabledObjLinearCoords;

			uUnitCount = 0;

			NEW_BLOCK(psFFGenCode, "Object Linear tex gen");

			/* Obj linear tex gen */
			while (uEnabledObjLinearCoords)
			{
				if (uEnabledObjLinearCoords & 0x1)
				{
					FFTNLGenLinearTexGenCode(psFFGenCode,
											 uUnitCount,
											 psFFTNLGenDesc->aubObjLinearCoordMask,
											 asOutputTexCoords,
											 psInputPosition,
											 FFGEN_STATE_OBJECTPLANES,
											 "Object Linear");
				}

				/* Shift mask */
				uEnabledObjLinearCoords >>= 1;

				uUnitCount++;
			}
		}

		/* Calculate R if required for sphere mapping or reflection mapping */
		if (ui32FFTNLEnables1 & (FFTNL_ENABLES1_TEXGENSPHEREMAP | FFTNL_ENABLES1_REFLECTIONMAP))
		{
			IMG_UINT32 i;

			FFGenReg *psNormal            = psFFGenCode->psNormal;
			FFGenReg *psVtxEyeVec         = psFFGenCode->psVtxEyeVec;
			FFGenReg *psTexGenR           = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 4, IMG_NULL);
			FFGenReg *psTemp              = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);
			FFGenReg *psImmediateFloatReg = &(psFFGenCode->sImmediateFloatReg);

		
			NEW_BLOCK(psFFGenCode, "Sphere + Reflection map common code");


			/* fdp3		r[DP3_DST(tT3)],		r[tNormal],		-r[tVtxEyeVec]; */

			SETSRCFLAG(1, USEASM_ARGFLAGS_NEGATE);
			SETDESTOFF(DP3_ADJUST(0));
			INST2(FDP3, psTexGenR, psNormal, psVtxEyeVec, "Get angle between eye vector and normal");

			/* 
			   fmul		r[tT2 + _X_],			r[tNormal + _X_],		r[tT3];
			   fmul		r[tT2 + _Y_],			r[tNormal + _Y_],		r[tT3];
			   fmul		r[tT2 + _Z_],			r[tNormal + _Z_],		r[tT3];
			   fmad		r[tT3 + _X_],			r[tT2 + _X_],			-#2.0,			-r[tVtxEyeVec + _X_]; 
			   fmad		r[tT3 + _Y_],			r[tT2 + _Y_],			-#2.0,			-r[tVtxEyeVec + _Y_]; 
			   fmad		r[tT3 + _Z_],			r[tT2 + _Z_],			-#2.0,			-r[tVtxEyeVec + _Z_]; 

			*/
			SETDESTOFF(0); SETSRCOFF(0, 0);
			INST2(FMUL, psTemp, psNormal, psTexGenR, " compute r: r[tT3] = r.x, r.y, r.z");

			SETDESTOFF(1); SETSRCOFF(0, 1);
			INST2(FMUL, psTemp, psNormal, psTexGenR, IMG_NULL);

			SETDESTOFF(2); SETSRCOFF(0, 2);
			INST2(FMUL, psTemp, psNormal, psTexGenR, IMG_NULL);

			SET_FLOAT_TWO(psImmediateFloatReg);

			for (i = 0; i < 3; i++)
			{
				SETSRCFLAG(1, USEASM_ARGFLAGS_NEGATE);
				SETSRCFLAG(2, USEASM_ARGFLAGS_NEGATE);
				SETDESTOFF(i); SETSRCOFF(0, i); SETSRCOFF(2, i);
				INST3(FMAD, psTexGenR, psTemp, psImmediateFloatReg, psVtxEyeVec, IMG_NULL);
			}

			/* Release this register */
			ReleaseReg(psFFGenCode, psTemp);
			
			/* hard code w to 1.0f */
			SETDESTOFF(3); SET_FLOAT_ONE(psImmediateFloatReg);
			INST1(MOV, psTexGenR, psImmediateFloatReg, IMG_NULL);
			
			/* Store this one */
			psFFGenCode->psTexGenR = psTexGenR;
		}


		if (uEnabledSphereMapCoords)
		{
			NEW_BLOCK(psFFGenCode, "Sphere Map tex gen");
		}

		/* Sphere map */
		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_TEXGENSPHEREMAP)
		{
			FFGenReg *psTexGenR           = psFFGenCode->psTexGenR;
			FFGenReg *psTexGenM           = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);
			FFGenReg *psImmediateFloatReg = &(psFFGenCode->sImmediateFloatReg);

			/*
			  mov		r[tT2 + _X_],			r[tT3 + _X_] ;
			  mov		r[tT2 + _Y_],			r[tT3 + _Y_] ;
			  fadd		r[tT2 + _Z_],			r[tT3 + _Z_],			#1.0;  
			  fdp3		r[DP3_DST(tT2)],		r[tT2],					r[tT2]; 
			  frsq		r[tT2],					r[tT2];
			  fmul		r[tT2],					r[tT2],					#0.5;   
			*/

			SETDESTOFF(0); SETSRCOFF(0, 0);
			INST1(MOV, psTexGenM, psTexGenR, "compute m: r[tT2] =((0.5f * 1/sqrt(r.x*r.x + r.y*r.y + (r.z + 1) * (r.z + 1)))");

			SETDESTOFF(1); SETSRCOFF(0, 1);
			INST1(MOV, psTexGenM, psTexGenR, IMG_NULL);

			SET_FLOAT_ONE(psImmediateFloatReg);

			SETDESTOFF(2); SETSRCOFF(0, 2);
			INST2(FADD, psTexGenM, psTexGenR, psImmediateFloatReg, IMG_NULL);
			
			SETDESTOFF(DP3_ADJUST(0));
			INST2(FDP3, psTexGenM, psTexGenM, psTexGenM, IMG_NULL);

			INST1(FRSQ, psTexGenM, psTexGenM, IMG_NULL);

			SET_FLOAT_HALF(psImmediateFloatReg);

			INST2(FMUL, psTexGenM, psTexGenM, psImmediateFloatReg, IMG_NULL);


			/* Sphere map gen */
			while (uEnabledSphereMapCoords)
			{
				IMG_UINT8 ubSphereMapCoordMask = psFFTNLGenDesc->aubSphereMapCoordMask[uUnitCount];

				if (uEnabledSphereMapCoords & 0x1)
				{
					IMG_UINT32 uCoordCount = 0;

					while (ubSphereMapCoordMask)
					{
						if (ubSphereMapCoordMask & 0x1)
						{
							/* fmad		o[il + _X_],				r[tT2],						r[tT3 + _X_],			#0.5;    */
	
							SETDESTOFF(uCoordCount); SETSRCOFF(1, uCoordCount);
							INST3(FMAD, &(asOutputTexCoords[uUnitCount]), psTexGenM, psTexGenR, psImmediateFloatReg, "generate sphere map texture coordinates");
						}

						/* Shift mask */
						ubSphereMapCoordMask >>= 1;

						uCoordCount++;
					}
				}

				/* Shift mask */
				uEnabledSphereMapCoords >>= 1;

				uUnitCount++;
			}

			/* Release the temps */
			ReleaseReg(psFFGenCode, psTexGenM);

			/* Reset unit count */
			uUnitCount = 0;
		}

		if (uEnabledNormalMapCoords)
		{
			NEW_BLOCK(psFFGenCode, "Normal Map tex gen");
		}

		/* normal map tex gen */
		while (uEnabledNormalMapCoords)
		{
			if (uEnabledNormalMapCoords & 0x1)
			{
				FFTNLGenTexPassthroughCode(psFFGenCode,
										   psFFTNLGenDesc->aubNormalMapCoordMask[uUnitCount],
										   &(asOutputTexCoords[uUnitCount]),
										   psFFGenCode->psNormal,
										   "Normal Map");
			}

			/* Shift mask */
			uEnabledNormalMapCoords >>= 1;

			uUnitCount++;
		}

		/* Reset unit count */
		uUnitCount = 0;
		
		if (uEnabledPositionMapCoords)
		{
			FFGenReg *psEyePosition = psFFGenCode->psEyePosition;

			NEW_BLOCK(psFFGenCode, "Position Map tex gen");

			/* normal map tex gen */
			while (uEnabledPositionMapCoords)
			{
				if (uEnabledPositionMapCoords & 0x1)
				{
					FFTNLGenTexPassthroughCode(psFFGenCode,
											   psFFTNLGenDesc->aubPositionMapCoordMask[uUnitCount],
											   &(asOutputTexCoords[uUnitCount]),
											   psEyePosition,
											   "Position Map");
				}
	
				/* Shift mask */
				uEnabledPositionMapCoords >>= 1;
	
				uUnitCount++;
			}
	
			/* Reset unit count */
			uUnitCount = 0;
		}

		if (uEnabledReflectionMapCoords)
		{
			NEW_BLOCK(psFFGenCode, "Reflection Map tex gen");
		}

		/* Reflection map gen */
		while (uEnabledReflectionMapCoords)
		{
			if (uEnabledReflectionMapCoords & 0x1)
			{
				FFTNLGenTexPassthroughCode(psFFGenCode,
										   psFFTNLGenDesc->aubReflectionMapCoordMask[uUnitCount],
										   &(asOutputTexCoords[uUnitCount]),
										   psFFGenCode->psTexGenR,
										   "Reflection Map");
			}

			/* Shift mask */
			uEnabledReflectionMapCoords >>= 1;

			uUnitCount++;
		}

		/* Reset unit count */
		uUnitCount = 0;


		if (uEnabledTextureMatrices)
		{
			NEW_BLOCK(psFFGenCode, "Texture matrix transformation");

			/* Count number of texture matrices */
			while (uEnabledTextureMatrices)
			{
				/* Shift mask */
				uEnabledTextureMatrices >>= 1;

				uUnitCount++;
			}

			/* Allocate space for the texture matrix array */
			AllocRegSpace(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_TEXTUREMATRIX, uUnitCount * FFTNL_SIZE_MATRIX_4X4, IMG_FALSE);

			/* Reset texture matrix enables */
			uEnabledTextureMatrices     = psFFTNLGenDesc->uEnabledTextureMatrices;

			/* Reset unit count */
			uUnitCount = 0;

			/* Texture matrix transformation */
			while (uEnabledTextureMatrices)
			{
				if (uEnabledTextureMatrices & 0x1)
				{
					/* Load the texture matrix */
					FFGenReg *psMatrix = GetReg(psFFGenCode, USEASM_REGTYPE_SECATTR, FFGEN_STATE_TEXTUREMATRIX, uUnitCount * 16, 16, "Texture Matrix");

					COMMENT(psFFGenCode, "Transform by texture matrix %d", uUnitCount);

					if ((uTexMatrixUseInputTexCoordRegsMask & (1 << uUnitCount)) && (ppsInputTexCoords[uUnitCount]))
					{
						/* Transform texture coordinate by texture matrix*/
						M4x4(psFFGenCode, &(asOutputTexCoords[uUnitCount]), ppsInputTexCoords[uUnitCount], psMatrix, 0, IMG_NULL);
					}
					else
					{
						/* Transform texture coordinate by texture matrix*/
						M4x4(psFFGenCode, &(asOutputTexCoords[uUnitCount]), &(asOutputTexCoords[uUnitCount]), psMatrix, 0, IMG_NULL);
					}

					/* Release any allocted regs */
					ReleaseReg(psFFGenCode, psMatrix);
				}

				/* Shift mask */
				uEnabledTextureMatrices >>= 1;
				
				uUnitCount++;
			}
		}
	}

	/*  Don't need tex gen R anymore */
	if (psFFGenCode->psTexGenR)
	{
		ReleaseReg(psFFGenCode, psFFGenCode->psTexGenR);
		psFFGenCode->psTexGenR = IMG_NULL;
	}

	/* Don't need normal anymore */
	if (psFFGenCode->psNormal)
	{
		ReleaseReg(psFFGenCode, psFFGenCode->psNormal);
		psFFGenCode->psNormal = IMG_NULL;
	}

	/* Don't need vertex eyevec anymore */
	if (psFFGenCode->psVtxEyeVec)
	{
		ReleaseReg(psFFGenCode, psFFGenCode->psVtxEyeVec);
		psFFGenCode->psVtxEyeVec = IMG_NULL;
	}
	if (psFFGenCode->psVtxEyeVecMag)
	{
		ReleaseReg(psFFGenCode, psFFGenCode->psVtxEyeVecMag);
		psFFGenCode->psVtxEyeVecMag = IMG_NULL;
	}
	if (psFFGenCode->psRSQVtxEyeVecMag)
	{
		ReleaseReg(psFFGenCode, psFFGenCode->psRSQVtxEyeVecMag);
		psFFGenCode->psRSQVtxEyeVecMag = IMG_NULL;
	}

	if (psFFGenCode->psEyePosition)
	{
		ReleaseReg(psFFGenCode, psFFGenCode->psEyePosition);
		psFFGenCode->psEyePosition = IMG_NULL;
	}
	
}

#if defined(OGL_MODULE) || !defined(FFGEN_UNIFLEX)
/******************************************************************************
 * Function Name: PatchIndexableSecondaries
 * Inputs       : psFFGenCode
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : Runs through list of indexable secondaries, and shuffles them
 *				  down to compect secondary space. Also adjusts instructions 
 *				  setting up the index register appropriately
 *****************************************************************************/
static IMG_VOID PatchIndexableSecondaries(FFGenCode *psFFGenCode)
{
	FFGenRegList *psRegList = psFFGenCode->psIndexableSecondaryList;
	UseInstructionList *psInstructionList = psFFGenCode->psIndexPatchUseInsts;
	FFGenReg *psReg;
	USE_INST *psInstruction;

	/* Loop though indexable secondary list */
	while(psRegList)
	{
		psReg = psRegList->psReg;
		psInstructionList = psFFGenCode->psIndexPatchUseInsts;
	
		/* Patch any instructions setting up index register to the base of this register */
		while(psInstructionList)
		{
			psInstruction = psInstructionList->psInstruction;
			
			if(psInstruction->asArg[1].uNumber == psReg->uOffset)
			{
				psInstruction->asArg[1].uNumber = psFFGenCode->uSecAttribSize;
			}
			
			psInstructionList = psInstructionList->psNext;
		}

		psReg->uOffset = psFFGenCode->uSecAttribSize;
		psFFGenCode->uSecAttribSize += psReg->uSizeInDWords;

		psRegList = psRegList->psNext;
	}
	

}
#endif /* defined(OGL_MODULE) || !defined(FFGEN_UNIFLEX) */


#if defined(SUPPORT_D3DM) || (defined(OGLES1_MODULE) && !defined(SGX_FEATURE_USE_UNLIMITED_PHASES) && !defined(FFGEN_UNIFLEX))
/******************************************************************************
 * Function Name: FFTNLGenEmitVertexEnd
 * Inputs       : psFFGenCode
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : Adds emitvtx.end.freep and the end of USE code
 *****************************************************************************/
static IMG_VOID FFTNLGenEmitVertexEnd(FFGenCode *psFFGenCode)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);
	FFGenReg *psImmediateIntReg  = &(psFFGenCode->sImmediateIntReg);

	NEW_BLOCK(psFFGenCode, "Emit Vertex Data and free partition");
	SET_FREEP();
	SET_END();
	SET_INT_ZERO(psImmediateIntReg);
	INST1(EMITVERTEX, psImmediateIntReg, psImmediateIntReg, "Emit Vertex and End");
}
#endif 


/******************************************************************************
 * Function Name: FFTNLGenerateCode
 * Inputs       : 
 * Outputs      : -
 * Returns      : FFGenCode
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL FFGenCode *FFTNLGenerateCode(FFGenContext	*psFFGenContext,
										  FFTNLGenDesc	*psFFTNLGenDesc,
										  IMG_BOOL		bUseDescriptions)
{
	IMG_UINT32 i;

#if defined(OGL_MODULE) || !defined(FFGEN_UNIFLEX)
	IMG_UINT32 uMaxNumLabels;
	USEASM_CONTEXT sUseasmContext;
	SGX_CORE_INFO  sTarget;
	FFGenUseasmState sUseasmState;
#endif /* defined(OGL_MODULE) || !defined(FFGEN_UNIFLEX) */

	/* Create memory for fixed function tnl code */
	FFGenCode *psFFGenCode = FFGENCalloc(psFFGenContext, sizeof(FFGenCode));

	/* Store program type */
	psFFGenCode->eProgramType = FFGENPT_TNL;

	/* Store init context */
	psFFGenCode->psFFGenContext = psFFGenContext;

	/* Store the description */
	psFFGenCode->psFFTNLGenDesc = psFFTNLGenDesc;

	/* Set initial indent */
	INCREASE_INDENT();

#if defined(STANDALONE) || defined(DEBUG)
	/* alloc mem for code */
	psFFGenCode->pszCode = FFGENCalloc(psFFGenContext, sizeof(IMG_CHAR) * 250000);
#endif	
	
	/* Store code generation method */
#if defined(FFGEN_UNIFLEX)
	psFFGenCode->eCodeGenMethod					= FFCGM_ONE_PASS;
#else
	psFFGenCode->eCodeGenMethod                 = psFFTNLGenDesc->eCodeGenMethod;
#endif
	psFFGenCode->eCodeGenFlags                  = psFFTNLGenDesc->eCodeGenFlags;

	/* Set inital secondary attribute allocations */
	psFFGenCode->uSecAttribStart				= psFFTNLGenDesc->uSecAttrStart;
	psFFGenCode->uSecAttribSize                 = psFFTNLGenDesc->uSecAttrStart;
	psFFGenCode->uHighSecAttribSize             = 0;
	psFFGenCode->uMaxSecAttribSize              = psFFTNLGenDesc->uSecAttrEnd;
	psFFGenCode->iSAConstBaseAddressAdjust      = -4;

	psFFGenCode->uMemoryConstantsSize = 0;

	psFFGenCode->psUseInsts       = IMG_NULL;
	psFFGenCode->psCurrentUseInst = IMG_NULL;
	psFFGenCode->puHWCode         = IMG_NULL;
	psFFGenCode->uNumInstructions = 0;

	/* Setup SA constant base address reg */
	psFFGenCode->sSAConstBaseAddress.eType           = USEASM_REGTYPE_SECATTR;
	psFFGenCode->sSAConstBaseAddress.uOffset         = psFFTNLGenDesc->uSecAttrConstBaseAddressReg;
	psFFGenCode->sSAConstBaseAddress.uSizeInDWords   = 1; 	

	/* Setup intermediate reg */
	psFFGenCode->sImmediateIntReg.eType             = USEASM_REGTYPE_IMMEDIATE;
	psFFGenCode->sImmediateIntReg.uSizeInDWords     = 1;

	psFFGenCode->sImmediateIntReg2.eType            = USEASM_REGTYPE_IMMEDIATE;
	psFFGenCode->sImmediateIntReg2.uSizeInDWords    = 1;

	psFFGenCode->sImmediateFloatReg.eType           = USEASM_REGTYPE_FPCONSTANT;
	psFFGenCode->sImmediateFloatReg.uSizeInDWords   = 1;

	psFFGenCode->sImmediateFloatReg2.eType          = USEASM_REGTYPE_FPCONSTANT;
	psFFGenCode->sImmediateFloatReg2.uSizeInDWords  = 1;

	psFFGenCode->sInternalReg.eType         = USEASM_REGTYPE_FPINTERNAL;
	psFFGenCode->sInternalReg.uSizeInDWords = 1;

	psFFGenCode->sLabelReg.eType            = USEASM_REGTYPE_LABEL;
	psFFGenCode->sLabelReg.uSizeInDWords    = 1;

	psFFGenCode->sRangeReg.eType            = USEASM_REGTYPE_TEMP;
	psFFGenCode->sRangeReg.uSizeInDWords    = 1;

	psFFGenCode->sClipPlaneReg.eType        = USEASM_REGTYPE_CLIPPLANE;
	psFFGenCode->sIndexLowReg.eType         = USEASM_REGTYPE_INDEX;
	psFFGenCode->sIndexLowReg.uOffset       = USEREG_INDEX_L;
	psFFGenCode->sIndexHighReg.eType        = USEASM_REGTYPE_INDEX;
	psFFGenCode->sIndexHighReg.uOffset      = USEREG_INDEX_H;
	psFFGenCode->sIndexLowHighReg.eType     = USEASM_REGTYPE_INDEX;
	psFFGenCode->sIndexLowHighReg.uOffset   = USEREG_INDEX_L | USEREG_INDEX_H;

	psFFGenCode->sIntSrcSelReg.eType		= USEASM_REGTYPE_INTSRCSEL;
	psFFGenCode->sIntSrcSelReg.uOffset		= USEASM_INTSRCSEL_ROUNDNEAREST;
	psFFGenCode->sIntSrcSelReg.uIndex		= USEREG_INDEX_NONE;

	psFFGenCode->sSwizzleXYZW.eType			= USEASM_REGTYPE_SWIZZLE;
	psFFGenCode->sSwizzleXYZW.uOffset		= USEASM_SWIZZLE(X, Y, Z, W);
	psFFGenCode->sSwizzleXYZW.uIndex		= USEREG_INDEX_NONE;
	

	/* Setup predicate regs */
	for (i = 0; i < 4; i++)
	{
		psFFGenCode->asPredicateRegs[i].eType           = USEASM_REGTYPE_PREDICATE;
		psFFGenCode->asPredicateRegs[i].uOffset         = i;
		psFFGenCode->asPredicateRegs[i].uSizeInDWords   = 1;
	}

	/* Init Data Read Channel and reg */
	psFFGenCode->sDRCReg.eType           = USEASM_REGTYPE_DRC;
	psFFGenCode->sDRCReg.uSizeInDWords   = 1;

	for(i = 0; i < EURASIA_USE_DRC_BANK_SIZE; i++)
	{
		psFFGenCode->abOutstandingDRC[i] = IMG_FALSE;
	}

	psFFGenCode->bUseRegisterDescriptions = bUseDescriptions;


	/* Generate the code */	

#ifdef FFGEN_UNIFLEX
	if(IF_FFGENCODE_UNIFLEX)
	{
		UNIFLEX_INST sMainLabel = {0};
		sMainLabel.eOpCode = UFOP_LABEL;
		sMainLabel.asSrc[0].eType = UFREG_TYPE_LABEL;
		sMainLabel.asSrc[0].uNum = USC_MAIN_LABEL_NUM;
		AddUniFlexInstructionToList(psFFGenCode, &sMainLabel);
	}
#endif

	FFTNLAssignOutputRegisters(psFFGenCode);

	FFTNLGenTransformation(psFFGenCode);

	FFTNLGenClipping(psFFGenCode);

	FFTNLGenColours(psFFGenCode);

	FFTNLGenFog(psFFGenCode);

	FFTNLGenVertexToEyeVector(psFFGenCode);

	FFTNLGenPoints(psFFGenCode);


	FFTNLGenOptimisedLighting(psFFGenCode);

	FFTNLGenTexturing(psFFGenCode);

	if (psFFGenCode->eCodeGenFlags & FFGENCGF_REDIRECT_OUTPUT_TO_INPUT)
	{
		FFTNLRedirectOutputRegisterWrites(psFFGenCode);
	
#if defined(OGLES1_MODULE) && !defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		{
			FFGenInstruction* psInst = &psFFGenCode->sInstruction;
			SET_END();
			INST(PADDING, "End of program");
		}
#endif
	}
#if defined(SUPPORT_D3DM) || (defined(OGLES1_MODULE) && !defined(SGX_FEATURE_USE_UNLIMITED_PHASES) && !defined(FFGEN_UNIFLEX))
	else
	{
		FFTNLGenEmitVertexEnd(psFFGenCode);
	}
#endif

	
#ifdef FFGEN_UNIFLEX
	if(IF_FFGENCODE_UNIFLEX)
	{
		UNIFLEX_INST sReturn = {0};
		sReturn.eOpCode = UFOP_RET;
		AddUniFlexInstructionToList(psFFGenCode, &sReturn);
	}
#endif

#if defined(OGL_MODULE) || !defined(FFGEN_UNIFLEX)

	if (psFFGenCode->eCodeGenMethod == FFCGM_TWO_PASS)
	{
		/* Compile the instructions to useasm if we're doing two pass */
		EncodeInstructionList(psFFGenCode);
	}

	PatchIndexableSecondaries(psFFGenCode);

	psFFGenCode->puHWCode = FFGENMalloc(psFFGenCode->psFFGenContext, sizeof(IMG_UINT32) * 2 * psFFGenCode->uNumInstructions);

	/* Number of labels is proportional to number of lights */
	//uMaxNumLabels = psFFGenCode->uNumLights * 10;
	uMaxNumLabels = 8 * 10;

	/* Setup useasm state */
	sUseasmState.piLabelArray = FFGENMalloc(psFFGenCode->psFFGenContext, sizeof(IMG_INT32) * uMaxNumLabels);

	/* Label array needs to be initialised */ 
	for (i = 0; i < uMaxNumLabels; i++)
	{
		sUseasmState.piLabelArray[i] = -1;
	}

	sUseasmState.psBaseInstr = psFFGenCode->psUseInsts;

	sUseasmState.psFFGenContext = psFFGenContext;

	/* Initialize the callbacks needed by useasm. */
	sUseasmContext.pvContext = &sUseasmState;
	sUseasmContext.pvLabelState = IMG_NULL;
	sUseasmContext.pfnRealloc = UseasmRealloc;
	sUseasmContext.pfnGetLabelAddress = UseAssemblerGetLabelAddress;
	sUseasmContext.pfnSetLabelAddress = UseAssemblerSetLabelAddress;
	sUseasmContext.pfnGetLabelName = UseAssemblerGetLabelName;
	sUseasmContext.pfnAssemblerError = AssemblerError;

#if defined(DEBUG)
	{
		IMG_UINT32 uNumInsts = 0;
		USE_INST   *psUseInsts = psFFGenCode->psUseInsts;

		/* Verify instruction list */
		while (psUseInsts)
		{
			if (psUseInsts->psNext)
			{
				psUseInsts->psNext->uOpcode++;
				psUseInsts->psNext->uOpcode--;
			}
			psUseInsts = psUseInsts->psNext;
			uNumInsts++;
		}
	}
#endif

	sTarget.eID = SGX_CORE_ID;
#if defined (SGX_CORE_REV)
	sTarget.uiRev = SGX_CORE_REV;
#else
	sTarget.uiRev = 0;		/* use head revision */
#endif
	/* Generate hw code */
	psFFGenCode->uNumHWInstructions = UseAssembler(UseAsmGetCoreDesc(&sTarget),
												   psFFGenCode->psUseInsts,
												   psFFGenCode->puHWCode,
												   0,
												   &sUseasmContext);

#if defined(STANDALONE) || defined(DEBUG)
	if (sUseasmState.piLabelArray[uMaxNumLabels - 1] != -1)
	{
		psFFGenContext->pfnPrint("Label array size possibly too small");
	}
#endif

	/* Free labels */
	FFGENFree(psFFGenContext, sUseasmState.piLabelArray);
#endif /* defined(OGL_MODULE) || !defined(FFGEN_UNIFLEX) */


	return psFFGenCode;
}

/******************************************************************************
 * Function Name: FFGENDestroyCode
 * Inputs       : psFFGenCode
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID FFGENDestroyCode(FFGenCode *psFFGenCode)
{
	USE_INST *psUseInst = psFFGenCode->psUseInsts;

	/* 
		NOTE:
			Don't free reg lists for constants, inputs and outputs 
			Dont free HW Code
	*/

	/* Destroy DebugMemFree temp list */
	DestroyRegList(psFFGenCode->psFFGenContext, psFFGenCode->psFreeTempList, IMG_TRUE);

	/* Destroy throwaway list */
	DestroyRegList(psFFGenCode->psFFGenContext, psFFGenCode->psThrowAwayList, IMG_TRUE);

	/* Destroy indexable secondary list */
	DestroyRegList(psFFGenCode->psFFGenContext, psFFGenCode->psIndexableSecondaryList, IMG_FALSE);
	
	/* Destroy indexable secondary patch instruction list */
	DestroyInstructionList(psFFGenCode->psFFGenContext, psFFGenCode->psIndexPatchUseInsts);

	DestroyLabelList(psFFGenCode);

	/* Destroy use instructions */
	while (psUseInst)
	{
		USE_INST   *psNext = psUseInst->psNext;

		FFGENFree(psFFGenCode->psFFGenContext, psUseInst);

		psUseInst = psNext;
	}

#if defined(STANDALONE) || defined(DEBUG)
	/* Free the source code */
	if (psFFGenCode->pszCode)
	{
		FFGENFree(psFFGenCode->psFFGenContext, psFFGenCode->pszCode);
	}
#endif

#if defined(OGL_LINESTIPPLE)
	if(psFFGenCode->psVertexPartitionInfo)
	{
		FFGENFree(psFFGenCode->psFFGenContext, psFFGenCode->psVertexPartitionInfo);
	}
#endif

	/* Finally free the structure. */
	FFGENFree(psFFGenCode->psFFGenContext, psFFGenCode);
}

/******************************************************************************
 End of file (codegen.c)
******************************************************************************/

