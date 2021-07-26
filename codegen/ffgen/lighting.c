/******************************************************************************
 * Name         : lighting.c
 * Author       : James McCarthy
 * Created      : 17/03/2006
 *
 * Copyright    : 2000-2006 by Imagination Technologies Limited.
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
 * $Log: lighting.c $
 **************************************************************************/
#include <stdio.h>
#include <stdarg.h>

#include "apidefs.h"
#include "codegen.h"
#include "macros.h"
#include "reg.h"
#include "inst.h"
#include "source.h"
#include "lighting.h"

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
static IMG_VOID FFTNLAddAmbientLight(FFGenCode *psFFGenCode,
							  IMG_UINT32  ui32FFTNLEnables1,
							  FFGenReg     *psInputColour,
							  FFGenReg     *psOutputColour,
							  FFGenReg     *psOutputSecColour,
							  FFGenReg     *psCurrentColour,
							  FFGenReg     *psLightSource,
							  FFGenReg     *psLightProduct,
							  FFGenReg     *psAmbientCoeff,
							  IMG_UINT32    uAmbientCoeffOffset)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);
	PVR_UNREFERENCED_PARAMETER(psOutputSecColour);

	if (ui32FFTNLEnables1 & FFTNL_ENABLES1_VERTCOLOURAMBIENT)
	{
		FFGenReg *psAmbientSource = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

		/* mul.rpt3	r[tAmbientSource],		pa[vPrimCol],				c[cLt0Ambi]; */
		SET_REPEAT_COUNT(3); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_AMBIENT);
		INST2(FMUL, psAmbientSource, psInputColour, psLightSource, "Calculate local ambient");  

		/*
		  fmad		o[oPrimCol + _X_],				r[tAmbientCoeff],		r[tAmbientSource + _X_],			o[tCurrentCol + _X_];
		  fmad		o[oPrimCol + _Y_],				r[tAmbientCoeff],		r[rAmbientSource + _Y_],			o[tCurrentCol + _Y_];
		  fmad		o[oPrimCol + _Z_],				r[tAmbientCoeff],		r[rAmbientSource + _Z_],			o[tCurrentCol + _Z_];
		*/
		SETDESTOFF(0); SETSRCOFF(0, uAmbientCoeffOffset); SETSRCOFF(1, 0); SETSRCOFF(2, 0); 
		INST3(FMAD, psOutputColour, psAmbientCoeff, psAmbientSource, psCurrentColour, "Calculate ambient components of color ");

		SETDESTOFF(1); SETSRCOFF(0, uAmbientCoeffOffset); SETSRCOFF(1, 1); SETSRCOFF(2, 1); 
		INST3(FMAD, psOutputColour, psAmbientCoeff, psAmbientSource, psCurrentColour, IMG_NULL);

		SETDESTOFF(2); SETSRCOFF(0, uAmbientCoeffOffset); SETSRCOFF(1, 2); SETSRCOFF(2, 2); 
		INST3(FMAD, psOutputColour, psAmbientCoeff, psAmbientSource, psCurrentColour, IMG_NULL);

		/* Release diffuse angle reg */
		ReleaseReg(psFFGenCode, psAmbientSource);
		psAmbientSource = IMG_NULL;
	}
	else
	{
		/*
		  fmad		o[oPrimCol + _X_],				r[tAmbientCoeff],		c[cLt0AmbiP + _X_],			o[tCurrentCol + _X_];
		  fmad		o[oPrimCol + _Y_],				r[tAmbientCoeff],		c[cLt0AmbiP + _Y_],			o[tCurrentCol + _Y_];
		  fmad		o[oPrimCol + _Z_],				r[tAmbientCoeff],		c[cLt0AmbiP + _Z_],			o[tCurrentCol + _Z_];
		*/
		SETDESTOFF(0); SETSRCOFF(0, uAmbientCoeffOffset); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTPRODUCT_AMBIENT + 0); SETSRCOFF(2, 0); 
		INST3(FMAD, psOutputColour, psAmbientCoeff, psLightProduct, psCurrentColour, "Calculate ambient components of color ");

		SETDESTOFF(1); SETSRCOFF(0, uAmbientCoeffOffset); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTPRODUCT_AMBIENT + 1); SETSRCOFF(2, 1); 
		INST3(FMAD, psOutputColour, psAmbientCoeff, psLightProduct, psCurrentColour, IMG_NULL);

		SETDESTOFF(2); SETSRCOFF(0, uAmbientCoeffOffset); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTPRODUCT_AMBIENT + 2); SETSRCOFF(2, 2); 
		INST3(FMAD, psOutputColour, psAmbientCoeff, psLightProduct, psCurrentColour, IMG_NULL);
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
static IMG_VOID FFTNLAddDiffuseLight(FFGenCode *psFFGenCode,
							  IMG_UINT32  ui32FFTNLEnables1,
							  FFGenReg     *psInputColour,
							  FFGenReg     *psOutputColour,
							  FFGenReg     *psOutputSecColour,
							  FFGenReg     *psCurrentColour,
							  FFGenReg     *psLightSource,
							  FFGenReg     *psLightProduct,
							  FFGenReg     *psDiffuseCoeff,
							  IMG_UINT32    uDiffuseCoeffOffset)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);
	PVR_UNREFERENCED_PARAMETER(psOutputSecColour);

	if (ui32FFTNLEnables1 & FFTNL_ENABLES1_VERTCOLOURDIFFUSE)
	{
		FFGenReg *psDiffuseSource = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

		/* mul.rpt3	r[tDiffuseSource],		pa[vPrimCol],				c[cLt0Ambi]; */
		SET_REPEAT_COUNT(3); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_DIFFUSE);
		INST2(FMUL, psDiffuseSource, psInputColour, psLightSource, "Calculate local diffuse");  

		/*
		  fmad		o[oPrimCol + _X_],				r[tDiffuseCoeff],		r[tDiffuseSource + _X_],			o[tCurrentCol + _X_];
		  fmad		o[oPrimCol + _Y_],				r[tDiffuseCoeff],		r[tDiffuseSource + _Y_],			o[tCurrentCol + _Y_];
		  fmad		o[oPrimCol + _Z_],				r[tDiffuseCoeff],		r[tDiffuseSource + _Z_],			o[tCurrentCol + _Z_];
		*/
		SETDESTOFF(0); SETSRCOFF(0, uDiffuseCoeffOffset); SETSRCOFF(1, 0); SETSRCOFF(2, 0); 
		INST3(FMAD, psOutputColour, psDiffuseCoeff, psDiffuseSource, psCurrentColour, "Calculate diffuse components of color ");

		SETDESTOFF(1); SETSRCOFF(0, uDiffuseCoeffOffset); SETSRCOFF(1, 1); SETSRCOFF(2, 1); 
		INST3(FMAD, psOutputColour, psDiffuseCoeff, psDiffuseSource, psCurrentColour, IMG_NULL);

		SETDESTOFF(2); SETSRCOFF(0, uDiffuseCoeffOffset); SETSRCOFF(1, 2); SETSRCOFF(2, 2); 
		INST3(FMAD, psOutputColour, psDiffuseCoeff, psDiffuseSource, psCurrentColour, IMG_NULL);

		/* Release diffuse angle reg */
		ReleaseReg(psFFGenCode, psDiffuseSource);
		psDiffuseSource = IMG_NULL;
	}
	else
	{
		/*
		  fmad		o[oPrimCol + _X_],				r[tDiffuseCoeff],		c[cLt0DiffP + _X_],			o[tCurrentCol + _X_];
		  fmad		o[oPrimCol + _Y_],				r[tDiffuseCoeff],		c[cLt0DiffP + _Y_],			o[tCurrentCol + _Y_];
		  fmad		o[oPrimCol + _Z_],				r[tDiffuseCoeff],		c[cLt0DiffP + _Z_],			o[tCurrentCol + _Z_];
		*/
		SETDESTOFF(0); SETSRCOFF(0, uDiffuseCoeffOffset); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTPRODUCT_DIFFUSE + 0); SETSRCOFF(2, 0); 
		INST3(FMAD, psOutputColour, psDiffuseCoeff, psLightProduct, psCurrentColour, "Calculate diffuse components of color ");

		SETDESTOFF(1); SETSRCOFF(0, uDiffuseCoeffOffset); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTPRODUCT_DIFFUSE + 1); SETSRCOFF(2, 1); 
		INST3(FMAD, psOutputColour, psDiffuseCoeff, psLightProduct, psCurrentColour, IMG_NULL);

		SETDESTOFF(2); SETSRCOFF(0, uDiffuseCoeffOffset); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTPRODUCT_DIFFUSE + 2); SETSRCOFF(2, 2); 
		INST3(FMAD, psOutputColour, psDiffuseCoeff, psLightProduct, psCurrentColour, IMG_NULL);
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
static IMG_VOID FFTNLAddSpecularLight(FFGenCode *psFFGenCode,
							   IMG_UINT32  ui32FFTNLEnables1,
							   FFGenReg     *psInputColour,
							   FFGenReg     *psOutputColour,
							   FFGenReg     *psOutputSecColour,
							   FFGenReg     *psLightSource,
							   FFGenReg     *psLightProduct,
							   FFGenReg     *psSpecularCoeff,
							   IMG_UINT32    uSpecularCoeffOffset)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);
	PVR_UNREFERENCED_PARAMETER(psOutputColour);

	/* 
	   Setup of psOutputSec colour will either write to PrimCol or Seccol depending on
	   the setup of FFTNL_ENABLES1_SEPARATESPECULAR in the enables 
	*/

	if (ui32FFTNLEnables1 & FFTNL_ENABLES1_VERTCOLOURSPECULAR)
	{
		FFGenReg *psSpecularSource = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

		/* mul.rpt3	r[tSpecularSource],		pa[vPrimCol],				c[cLt0Ambi]; */
		SET_REPEAT_COUNT(3); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_SPECULAR);
		INST2(FMUL, psSpecularSource, psInputColour, psLightSource, "Calculate local specular");

		/*
		  fmad		o[oPrimCol + _X_],				r[tSpecularCoeff],		r[tSpecularSource + _X_],			o[oPrimCol + _X_];
		  fmad		o[oPrimCol + _Y_],				r[tSpecularCoeff],		r[tSpecularSource + _Y_],			o[oPrimCol + _Y_];
		  fmad		o[oPrimCol + _Z_],				r[tSpecularCoeff],		r[tSpecularSource + _Z_],			o[oPrimCol + _Z_];
		*/
		SETDESTOFF(0); SETSRCOFF(0, uSpecularCoeffOffset); SETSRCOFF(1, 0); SETSRCOFF(2, 0); 
		INST3(FMAD, psOutputSecColour, psSpecularCoeff, psSpecularSource, psOutputSecColour, "Calculate specular components of color ");

		SETDESTOFF(1); SETSRCOFF(0, uSpecularCoeffOffset); SETSRCOFF(1, 1); SETSRCOFF(2, 1); 
		INST3(FMAD, psOutputSecColour, psSpecularCoeff, psSpecularSource, psOutputSecColour, IMG_NULL);

		SETDESTOFF(2); SETSRCOFF(0, uSpecularCoeffOffset); SETSRCOFF(1, 2); SETSRCOFF(2, 2); 
		INST3(FMAD, psOutputSecColour, psSpecularCoeff, psSpecularSource, psOutputSecColour, IMG_NULL);

		/* Release specular angle reg */
		ReleaseReg(psFFGenCode, psSpecularSource);
		psSpecularSource = IMG_NULL;
	}
	else
	{
		/*
		  fmad		o[oPrimCol + _X_],				r[tSpecularCoeff],		c[cLt0SpecP + _X_],			o[oPrimCol + _X_];
		  fmad		o[oPrimCol + _Y_],				r[tSpecularCoeff],		c[cLt0SpecP + _Y_],			o[oPrimCol + _Y_];
		  fmad		o[oPrimCol + _Z_],				r[tSpecularCoeff],		c[cLt0SpecP + _Z_],			o[oPrimCol + _Z_];
		*/
		SETDESTOFF(0); SETSRCOFF(0, uSpecularCoeffOffset); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTPRODUCT_SPECULAR + 0); SETSRCOFF(2, 0); 
		INST3(FMAD, psOutputSecColour, psSpecularCoeff, psLightProduct, psOutputSecColour, "Calculate specular components of color ");

		SETDESTOFF(1); SETSRCOFF(0, uSpecularCoeffOffset); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTPRODUCT_SPECULAR + 1); SETSRCOFF(2, 1); 
		INST3(FMAD, psOutputSecColour, psSpecularCoeff, psLightProduct, psOutputSecColour, IMG_NULL);

		SETDESTOFF(2); SETSRCOFF(0, uSpecularCoeffOffset); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTPRODUCT_SPECULAR + 2); SETSRCOFF(2, 2); 
		INST3(FMAD, psOutputSecColour, psSpecularCoeff, psLightProduct, psOutputSecColour, IMG_NULL);
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
static IMG_VOID FFTNLGenPointInfiniteLighting(FFGenCode *psFFGenCode,
									   IMG_UINT32  ui32FFTNLEnables1,
									   FFGenReg     *psOutputSecColour,
									   FFGenReg     *psInputColour,
									   FFGenReg     *psOutputColour,
									   FFGenReg     *psCurrentColour,
									   FFGenReg     *psMaterial,
									   FFGenReg     *psLabel,
									   FFGenReg     *psLightSource,
									   FFGenReg     *psLightProduct,
									   IMG_BOOL      bSpecularPresent,
									   IMG_BOOL      bLightProductAlsoContainsAmbient)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

	FFGenReg *psNormal = psFFGenCode->psNormal;

	IMG_BOOL bNormaliseSpecHalfAngle = IMG_FALSE;

	FFGenReg  *psVtxLitVec     = IMG_NULL;
	FFGenReg  *psSpecHalfAngle = IMG_NULL;

	FFGenReg *psTemp1,     *psVtxLitVecMagSq,  *psSpotAttenProd;
	FFGenReg *psDiffAngle, *psLtCoeffs;

	FFGenReg *psVtxEyeVec = psFFGenCode->psVtxEyeVec;

	FFGenReg *psImmediateFloatReg  = &(psFFGenCode->sImmediateFloatReg);
	FFGenReg *psImmediateFloatReg2 = &(psFFGenCode->sImmediateFloatReg2);

	/* Get predicate regs */
	FFGenReg *psPredReg0        = &(psFFGenCode->asPredicateRegs[0]);

	IMG_BOOL bLightPosNormalised = IMG_TRUE;

	/* 
	   Get a label which we'll place at the end of the lighting code so we
	   can skip to it at any time 
	*/
	IMG_UINT32 uEndOfLightLabel = GetLabel(psFFGenCode, "EndOfLight"); 

	psTemp1 = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 4, IMG_NULL);

	if (!bLightPosNormalised)
	{
		/* get temp for vertex to light vector */
		psVtxLitVec = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

		/* get temp for vertex to light vector magnitude squared */
		psVtxLitVecMagSq = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

		/* fdp3			r[DP3_DST(tVtxLitVecMagSq)],			c[cLt0Pos],				c[cLt0Pos] ; */
		SETDESTOFF(DP3_ADJUST(0));
		SETSRCOFF(0, FFTNL_OFFSETS_LIGHTSOURCE_POSITION); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_POSITION);
		INST2(FDP3, psVtxLitVecMagSq, psLightSource, psLightSource, 
			  "Normalise vertex to light vector");

		/*
		  frsq			r[tT1],				     	r[tVtxLitVecMagSq] ;
		  fmul			r[tVtxLitVec + _X_],		r[tT1],				 c[cLt0Pos + _X_] ;
		  fmul			r[tVtxLitVec + _Y_],		r[tT1],				 c[cLt0Pos + _Y_] ;
		  fmul			r[tVtxLitVec + _Z_],		r[tT1],				 c[cLt0Pos + _Z_] ;
		*/

		INST1(FRSQ, psTemp1, psVtxLitVecMagSq, IMG_NULL);

		SETDESTOFF(0);  SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_POSITION + 0); 
		INST2(FMUL, psVtxLitVec, psTemp1, psLightSource,  IMG_NULL);

		SETDESTOFF(1);  SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_POSITION + 1); 
		INST2(FMUL, psVtxLitVec, psTemp1, psLightSource, IMG_NULL);

		SETDESTOFF(2);  SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_POSITION + 2); 
		INST2(FMUL, psVtxLitVec, psTemp1, psLightSource, IMG_NULL);

		/* Release reg for vertex lit vec magnitude squared */
		ReleaseReg(psFFGenCode, psVtxLitVecMagSq);
		psVtxLitVecMagSq = IMG_NULL;
	}

	/* For infinite point lights there is no attenuation */
	SET_FLOAT_ONE(psImmediateFloatReg2);
	psSpotAttenProd = psImmediateFloatReg2;
	
	/* Get temp for diffuse angle calculation */
	psDiffAngle = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	if (!bLightPosNormalised)
	{
		/* fdp3			r[DP3_DST(tDiffAngle)],		r[tVtxLitVec],		r[tNormal];        // f_i (n dot VP_pli) */
		SETDESTOFF(DP3_ADJUST(0)); 
		INST2(FDP3, psDiffAngle, psVtxLitVec, psNormal, "Calculate angle between normal and vertex to light vector");
	}
	else
	{
		/* fdp3			r[DP3_DST(tDiffAngle)],		c[cLt0Pos],		r[tNormal];        // f_i (n dot VP_pli) */
		SETDESTOFF(DP3_ADJUST(0)); SETSRCOFF(0, FFTNL_OFFSETS_LIGHTSOURCE_POSITION_NORMALISED);
		INST2(FDP3, psDiffAngle, psLightSource, psNormal, "Calculate angle between normal and normalised vertex to light vector");
	}

	if (bSpecularPresent)
	{
		/* fmov.testn|z	p0,							r[tDiffAngle] ; */
		SET_TESTPRED_NEGATIVE_OR_ZERO();
		INST1(FMOV, psPredReg0, psDiffAngle, "Test result - !! shift this test into prev instruction when HW and compiler fixed to support this !!");

		IF_PRED(psFFGenCode, psPredReg0, "AmbientOnly", "Check if it's worth calculating the diffuse and specular components");

		FFTNLAddAmbientLight(psFFGenCode,
							 ui32FFTNLEnables1,
							 psInputColour,
							 psOutputColour,
							 psOutputSecColour,
							 psCurrentColour,
							 psLightSource,
							 psLightProduct,
							 psSpotAttenProd,
							 0);

#ifdef FFGEN_UNIFLEX
		if(IF_FFGENCODE_UNIFLEX)
		{
			ELSE_PRED(psFFGenCode, IMG_NULL, IMG_NULL);
		}
		else
#endif
		{
			/* br uEndOfLightLabel: */
			psLabel->uOffset = uEndOfLightLabel;
			INST0(BR, psLabel, "Move onto next light");

			END_PRED(psFFGenCode, IMG_NULL);
		}
	}
	else
	{
		SET_FLOAT_ZERO(psImmediateFloatReg);
		INST2(FMAX, psDiffAngle, psDiffAngle, psImmediateFloatReg, "Clamp diff angle to 0.0");
	}


	if (bSpecularPresent)
	{
		psSpecHalfAngle = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);
		
		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_LOCALVIEWER)
		{
			if (!bLightPosNormalised)
			{
				/* fadd.rpt3	r[tSpecHalfAngle],				r[tVtxLitVec],					r[tVtxEyeVec] */
				SET_REPEAT_COUNT(3);
				INST2(FADD, psSpecHalfAngle, psVtxLitVec, psVtxEyeVec, "Local viewer enabled : h_i = (VP_pli + VP_e)");
			}
			else
			{
				/* fadd.rpt3	r[tSpecHalfAngle],				c[cLt0Pos],					r[tVtxEyeVec] */
				SET_REPEAT_COUNT(3);
				SETSRCOFF(0, FFTNL_OFFSETS_LIGHTSOURCE_POSITION); 
				INST2(FADD, psSpecHalfAngle, psLightSource, psVtxEyeVec, "Local viewer enabled : h_i = (VP_pli + VP_e)");
			}

			bNormaliseSpecHalfAngle = IMG_TRUE;
		}
		else
		{
			/* This can be optimised out */
			SET_REPEAT_COUNT(3);
			SETSRCOFF(0, FFTNL_OFFSETS_LIGHTSOURCE_HALFVECTOR); 
			INST1(MOV, psSpecHalfAngle, psLightSource, "Load light half angle");

			bNormaliseSpecHalfAngle = IMG_FALSE;
		}
	}


	if (!bLightPosNormalised)
	{
		/* Release the vertex lit vector */
		ReleaseReg(psFFGenCode, psVtxLitVec);
		psVtxLitVec = IMG_NULL;
	}

	if (bSpecularPresent)
	{
		if (bNormaliseSpecHalfAngle)
		{
			/*
			  fdp3		r[DP3_DST(tT1)],				r[tSpecHalfAngle],				r[tSpecHalfAngle];
			  frsq		r[tT1],							r[tT1];
			  fmul		r[tSpecHalfAngle + _X_],		r[tSpecHalfAngle + _X_],		r[tT1];
			  fmul		r[tSpecHalfAngle + _Y_],		r[tSpecHalfAngle + _Y_],		r[tT1];
			  fmul		r[tSpecHalfAngle + _Z_],		r[tSpecHalfAngle + _Z_],		r[tT1];
			*/

			SETDESTOFF(DP3_ADJUST(0)); 
			INST2(FDP3, psTemp1, psSpecHalfAngle, psSpecHalfAngle, "Normalise specular half angle");

			INST1(FRSQ, psTemp1, psTemp1, IMG_NULL);

			SETDESTOFF(0); SETSRCOFF(0, 0);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);

			SETDESTOFF(1); SETSRCOFF(0, 1);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);

			SETDESTOFF(2); SETSRCOFF(0, 2);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);
		}


		/* 	fdp3		r[DP3_DST(tSpecHalfAngle)],		r[tNormal],						r[tSpecHalfAngle]; */
		SETDESTOFF(DP3_ADJUST(0)); 
		INST2(FDP3, psSpecHalfAngle, psNormal, psSpecHalfAngle, "Calc specular half angle ");

		/* fmov.testp|z	p0,							r[tSpecHalfAngle] ; */
		SET_TESTPRED_POSITIVE_OR_ZERO();
		INST1(FMOV, psPredReg0, psSpecHalfAngle, "Set predicate for specular test - MOVE INTO ABOVE INSTRUCTION WHEN SUPPORTED");

		IF_PRED(psFFGenCode, psPredReg0, "SpecLightCoeff", "Do we need to calc specular coeff?");

		/*
		  flog		r[tSpecHalfAngle],				r[tSpecHalfAngle] ;
		  fmul		r[tSpecHalfAngle],				r[tSpecHalfAngle],				c[cMatShin] ; 
		  fexp		r[tSpecHalfAngle],				r[tSpecHalfAngle] ; 
		*/

		INST1(FLOG, psSpecHalfAngle, psSpecHalfAngle, "Calculate specular coeff");

		SETSRCOFF(1, FFTNL_OFFSETS_MATERIAL_SHININESS);
		INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psMaterial, IMG_NULL);

		INST1(FEXP, psSpecHalfAngle, psSpecHalfAngle, IMG_NULL);

		ELSE_PRED(psFFGenCode, "NoSpecLightCoeff", IMG_NULL);

		/* 	mov			r[tSpecHalfAngle],				#0.0 ; */
		SET_FLOAT_ZERO(psImmediateFloatReg);
		INST1(MOV, psSpecHalfAngle, psImmediateFloatReg, "Set specular coeff to 0");

		END_PRED(psFFGenCode, IMG_NULL);
	}

	psLtCoeffs = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	if (!bLightProductAlsoContainsAmbient)
	{
		/*
		  fmov		r[tLtCoeffs],				#1.0;
		*/
		INST1(MOV, psLtCoeffs, psSpotAttenProd, "Load reg with 1.0 to get round reg restrictions below");

		FFTNLAddAmbientLight(psFFGenCode,
							 ui32FFTNLEnables1,
							 psInputColour, 
							 psOutputColour,
							 psOutputSecColour, 
							 psCurrentColour,
							 psLightSource,
							 psLightProduct,
							 psLtCoeffs,
							 0);

		/* The current colour is now the output colour */
		psCurrentColour = psOutputColour;
	}

	/* Release spot attenuation product */
	ReleaseReg(psFFGenCode, psSpotAttenProd);
	psSpotAttenProd = IMG_NULL;

	FFTNLAddDiffuseLight(psFFGenCode,
						 ui32FFTNLEnables1,
						 psInputColour, 
						 psOutputColour,
						 psOutputSecColour, 
						 psCurrentColour,
						 psLightSource,
						 psLightProduct,
						 psDiffAngle,
						 0);

	if (bSpecularPresent)
	{
		FFTNLAddSpecularLight(psFFGenCode,
							  ui32FFTNLEnables1,
							  psInputColour, 
							  psOutputColour,
							  psOutputSecColour, 
							  psLightSource,
							  psLightProduct,
							  psSpecHalfAngle,
							  0);
	}

	/* Release diffuse angle reg */
	ReleaseReg(psFFGenCode, psDiffAngle);
	psDiffAngle = IMG_NULL;

	if (bSpecularPresent)
	{
		/* Release specular half angle */
		ReleaseReg(psFFGenCode, psSpecHalfAngle);
		psSpecHalfAngle = IMG_NULL;
	}

	ReleaseReg(psFFGenCode, psLtCoeffs);
	psLtCoeffs = IMG_NULL;

#ifdef FFGEN_UNIFLEX
	if(IF_FFGENCODE_UNIFLEX)
	{
		if(bSpecularPresent)
			END_PRED(psFFGenCode, IMG_NULL);
	}
	else
#endif
	{
		/* Insert label marking end of this lights code */
		psLabel->uOffset = uEndOfLightLabel;
		INST0(LABEL, psLabel, "End of light");
	}

	/* Release temp register */
	ReleaseReg(psFFGenCode, psTemp1);
	psTemp1 = IMG_NULL;
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
static IMG_VOID FFTNLGenPointLocalLighting(FFGenCode *psFFGenCode,
									IMG_UINT32  ui32FFTNLEnables1,
									FFGenReg     *psOutputSecColour,
									FFGenReg     *psInputColour,
									FFGenReg     *psOutputColour,
									FFGenReg     *psCurrentColour,
									FFGenReg     *psMaterial,
									FFGenReg     *psLabel,
									FFGenReg     *psLightSource,
									FFGenReg     *psLightProduct,
									IMG_BOOL      bSpecularPresent)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

	FFGenReg *psNormal = psFFGenCode->psNormal;

	FFGenReg *psEyePos = psFFGenCode->psEyePosition;

	IMG_BOOL bNormaliseSpecHalfAngle = IMG_FALSE;

	FFGenReg  *psVtxLitVec     = IMG_NULL;
	FFGenReg  *psSpecHalfAngle = IMG_NULL;
	FFGenReg  *psDistVec = IMG_NULL; //
	FFGenReg  *psDistAtten = IMG_NULL; //

	FFGenReg *psTemp1,     *psVtxLitVecMagSq,  *psSpotAttenProd;
	FFGenReg *psDiffAngle, *psLtCoeffs;

	FFGenReg *psVtxEyeVec = psFFGenCode->psVtxEyeVec;

	FFGenReg *psImmediateFloatReg  = &(psFFGenCode->sImmediateFloatReg);

	/* Get predicate regs */
	FFGenReg *psPredReg0        = &(psFFGenCode->asPredicateRegs[0]);

	/* 
	   Get a label which we'll place at the end of the lighting code so we
	   can skip to it at any time 
	*/
	IMG_UINT32 uEndOfLightLabel = GetLabel(psFFGenCode, "EndOfLight"); 

	psTemp1 = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* get temp for vertex to light vector */
	psVtxLitVec = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

	/* get temp for vertex to light vector magnitude squared */
	psVtxLitVecMagSq = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* 	//fsub.rpt3		r[tVtxLitVec],				c[cLt0Pos],				r[tEyePos]; */
	/* 	fadd.rpt3		r[tVtxLitVec],				-r[tEyePos];            c[cLt0Pos] */
	SET_REPEAT_COUNT(3);
	SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_POSITION);
	SETSRCFLAG(0, USEASM_ARGFLAGS_NEGATE);
	INST2(FADD, psVtxLitVec, psEyePos, psLightSource, "Get vertex to light vector (fsub not support sa as 1st src so changed to add)");


	/* fdp3			r[DP3_DST(tVtxLitVecMagSq)],		r[tVtxLitVec],				r[tVtxLitVec] ;	*/
	SETDESTOFF(DP3_ADJUST(0));
	INST2(FDP3, psVtxLitVecMagSq, psVtxLitVec, psVtxLitVec, "Normalise vertex to light vector");

	/*
	  frsq			r[tT1],				     	r[tVtxLitVecMagSq] ;
	  fmul			r[tVtxLitVec + _X_],		r[tT1],				r[tVtxLitVec + _X_] ;
	  fmul			r[tVtxLitVec + _Y_],		r[tT1],				r[tVtxLitVec + _Y_] ;
	  fmul			r[tVtxLitVec + _Z_],		r[tT1],				r[tVtxLitVec + _Z_] ;
	*/

	INST1(FRSQ, psTemp1, psVtxLitVecMagSq, IMG_NULL);

	SETDESTOFF(0);  SETSRCOFF(1, 0); 
	INST2(FMUL, psVtxLitVec, psTemp1, psVtxLitVec, IMG_NULL);

	SETDESTOFF(1);  SETSRCOFF(1, 1); 
	INST2(FMUL, psVtxLitVec, psTemp1, psVtxLitVec, IMG_NULL);

	SETDESTOFF(2);  SETSRCOFF(1, 2); 
	INST2(FMUL, psVtxLitVec, psTemp1, psVtxLitVec, IMG_NULL);


	/* get reg for distance vector */
	psDistVec = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);
	
	/*
	  mov			r[tDistVec + _X_],			#1.0 ;
	  fmul			r[tDistVec + _Y_],			r[tVtxLitVecMagSq],				r[tT1] ;
	  fmov			r[tDistVec + _Z_],			r[tVtxLitVecMagSq] ;
	*/

	SET_FLOAT_ONE(psImmediateFloatReg);
	SETDESTOFF(0);
	INST1(MOV, psDistVec, psImmediateFloatReg, "Setup distance vector  = {1, |VP_pli|, SQ(|VP_pli|)");

	SETDESTOFF(1);
	INST2(FMUL, psDistVec, psVtxLitVecMagSq, psTemp1, IMG_NULL);

	SETDESTOFF(2);
	INST1(FMOV, psDistVec, psVtxLitVecMagSq, IMG_NULL);

	/* Get distance attenuation register */
	psDistAtten = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* fdp3	r[tDistAtten],		r[tDistVec],		c[cLt0Atten]; */
	SETDESTOFF(DP3_ADJUST(0)); 
	SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_CONSTANTATTENUATION);
	INST2(FDP3, psDistAtten, psDistVec, psLightSource, "Calculate distance attenuation");

	/* Relase distance vector */
	ReleaseReg(psFFGenCode, psDistVec);
	psDistVec = IMG_NULL;

	/* frcp			r[tDistAtten],				r[tDistAtten];         // (att_i); */
	INST1(FRCP, psDistAtten, psDistAtten, "(att_i)");

	/* Release reg for vertex lit vec magnitude squared */
	ReleaseReg(psFFGenCode, psVtxLitVecMagSq);
	psVtxLitVecMagSq = IMG_NULL;


	/* For local lights the distance is the only attenuation that is calculated */
	psSpotAttenProd = psDistAtten;

	/* Get temp for diffuse angle calculation */
	psDiffAngle = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* fdp3			r[DP3_DST(tDiffAngle)],		r[tVtxLitVec],		r[tNormal];        // f_i (n dot VP_pli) */
	SETDESTOFF(DP3_ADJUST(0)); 
	INST2(FDP3, psDiffAngle, psVtxLitVec, psNormal, "Calculate angle between normal and vertex to light vector");


	if (bSpecularPresent)
	{
		/* fmov.testn|z	p0,							r[tDiffAngle] ; */
		SET_TESTPRED_NEGATIVE_OR_ZERO();
		INST1(FMOV, psPredReg0, psDiffAngle, "Test result - !! shift this test into prev instruction when HW and compiler fixed to support this !!");

		IF_PRED(psFFGenCode, psPredReg0, "AmbientOnly", "Check if it's worth calculating the diffuse and specular components");

		FFTNLAddAmbientLight(psFFGenCode,
							 ui32FFTNLEnables1,
							 psInputColour,
							 psOutputColour,
							 psOutputSecColour,
							 psCurrentColour,
							 psLightSource,
							 psLightProduct,
							 psSpotAttenProd,
							 0);

#ifdef FFGEN_UNIFLEX
		if(IF_FFGENCODE_UNIFLEX)
		{
			ELSE_PRED(psFFGenCode, IMG_NULL, IMG_NULL);
		}
		else
#endif
		{
			/* br uEndOfLightLabel: */
			psLabel->uOffset = uEndOfLightLabel;
			INST0(BR, psLabel, "Move onto next light");

			END_PRED(psFFGenCode, IMG_NULL);
		}
	}
	else
	{
		SET_FLOAT_ZERO(psImmediateFloatReg);
		INST2(FMAX, psDiffAngle, psDiffAngle, psImmediateFloatReg, "Clamp diff angle to 0.0");
	}

	if (bSpecularPresent)
	{
		psSpecHalfAngle = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_LOCALVIEWER)
		{
			/* fadd.rpt3	r[tSpecHalfAngle],				r[tVtxLitVec],					r[tVtxEyeVec] */
			SET_REPEAT_COUNT(3);
			INST2(FADD, psSpecHalfAngle, psVtxLitVec, psVtxEyeVec, "Local viewer enabled : h_i = (VP_pli + VP_e)");

			bNormaliseSpecHalfAngle = IMG_TRUE;
		}
		else
		{
			/* fadd.rpt3	r[tSpecHalfAngle],				r[tVtxLitVec],					c[cLitVec001] */
			SET_FLOAT_ZERO(psImmediateFloatReg);
			SETDESTOFF(0); SETSRCOFF(0, 0); 
			INST2(FADD, psSpecHalfAngle, psVtxLitVec, psImmediateFloatReg, "Local viewer disabled :h_i = (VP_pli + {0,0,1})");

			SETDESTOFF(1); SETSRCOFF(0, 1); 
			INST2(FADD, psSpecHalfAngle, psVtxLitVec, psImmediateFloatReg, IMG_NULL);

			SET_FLOAT_ONE(psImmediateFloatReg);
			SETDESTOFF(2); SETSRCOFF(0, 2); 
			INST2(FADD, psSpecHalfAngle, psVtxLitVec, psImmediateFloatReg, IMG_NULL);

			bNormaliseSpecHalfAngle = IMG_TRUE;
		}
	}

	/* Release the vertex lit vector */
	ReleaseReg(psFFGenCode, psVtxLitVec);
	psVtxLitVec = IMG_NULL;

	if (bSpecularPresent)
	{
		if (bNormaliseSpecHalfAngle)
		{
			/*
			  fdp3		r[DP3_DST(tT1)],				r[tSpecHalfAngle],				r[tSpecHalfAngle];
			  frsq		r[tT1],							r[tT1];
			  fmul		r[tSpecHalfAngle + _X_],		r[tSpecHalfAngle + _X_],		r[tT1];
			  fmul		r[tSpecHalfAngle + _Y_],		r[tSpecHalfAngle + _Y_],		r[tT1];
			  fmul		r[tSpecHalfAngle + _Z_],		r[tSpecHalfAngle + _Z_],		r[tT1];
			*/
			
			SETDESTOFF(DP3_ADJUST(0)); 
			INST2(FDP3, psTemp1, psSpecHalfAngle, psSpecHalfAngle, "Normalise specular half angle");
			
			INST1(FRSQ, psTemp1, psTemp1, IMG_NULL);

			SETDESTOFF(0); SETSRCOFF(0, 0);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);
			
			SETDESTOFF(1); SETSRCOFF(0, 1);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);
			
			SETDESTOFF(2); SETSRCOFF(0, 2);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);
		}


		/* 	fdp3		r[DP3_DST(tSpecHalfAngle)],		r[tNormal],						r[tSpecHalfAngle]; */
		SETDESTOFF(DP3_ADJUST(0)); 
		INST2(FDP3, psSpecHalfAngle, psNormal, psSpecHalfAngle, "Calc specular half angle ");

		/* fmov.testp|z	p0,							r[tSpecHalfAngle] ; */
		SET_TESTPRED_POSITIVE_OR_ZERO();
		INST1(FMOV, psPredReg0, psSpecHalfAngle, "Set predicate for specular test - MOVE INTO ABOVE INSTRUCTION WHEN SUPPORTED");

		IF_PRED(psFFGenCode, psPredReg0, "SpecLightCoeff", "Do we need to calc specular coeff?");

		/*
		  flog		r[tSpecHalfAngle],				r[tSpecHalfAngle] ;
		  fmul		r[tSpecHalfAngle],				r[tSpecHalfAngle],				c[cMatShin] ; 
		  fexp		r[tSpecHalfAngle],				r[tSpecHalfAngle] ; 
		*/

		INST1(FLOG, psSpecHalfAngle, psSpecHalfAngle, "Calculate specular coeff");

		SETSRCOFF(1, FFTNL_OFFSETS_MATERIAL_SHININESS);
		INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psMaterial, IMG_NULL);

		INST1(FEXP, psSpecHalfAngle, psSpecHalfAngle, IMG_NULL);

		ELSE_PRED(psFFGenCode, "NoSpecLightCoeff", IMG_NULL);

		/* 	mov			r[tSpecHalfAngle],				#0.0 ; */
		SET_FLOAT_ZERO(psImmediateFloatReg);
		INST1(MOV, psSpecHalfAngle, psImmediateFloatReg, "Set specular coeff to 0");

		END_PRED(psFFGenCode, IMG_NULL);
	}

	psLtCoeffs = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

	/*
	  fmul		r[tLtCoeffs + _X_],				#1.0                    r[tSpotAttenProd];
	  fmul		r[tLtCoeffs + _Y_],				r[tDiffangle],   		r[tSpotAttenProd];
	  fmul		r[tLtCoeffs + _Z_],				r[tSpecHalfAngle],		r[tSpotAttenProd];
	*/
	SETDESTOFF(0);
	INST1(MOV, psLtCoeffs, psSpotAttenProd, "Attenuate light coefficients");

	SETDESTOFF(1); 
	INST2(FMUL, psLtCoeffs, psDiffAngle, psSpotAttenProd, IMG_NULL);

	if (bSpecularPresent)
	{
		SETDESTOFF(2); 
		INST2(FMUL, psLtCoeffs, psSpecHalfAngle, psSpotAttenProd, IMG_NULL);
	}

	/* Release diffuse angle reg */
	ReleaseReg(psFFGenCode, psDiffAngle);
	psDiffAngle = IMG_NULL;

	if (bSpecularPresent)
	{
		/* Release specular half angle */
		ReleaseReg(psFFGenCode, psSpecHalfAngle);
		psSpecHalfAngle = IMG_NULL;
	}

	/* Release spot attenuation product */
	ReleaseReg(psFFGenCode, psSpotAttenProd);
	psSpotAttenProd = IMG_NULL;

	FFTNLAddAmbientLight(psFFGenCode,
						 ui32FFTNLEnables1,
						 psInputColour,
						 psOutputColour,
						 psOutputSecColour,
						 psCurrentColour,
						 psLightSource,
						 psLightProduct,
						 psLtCoeffs,
						 0);

	FFTNLAddDiffuseLight(psFFGenCode,
						 ui32FFTNLEnables1,
						 psInputColour, 
						 psOutputColour,
						 psOutputSecColour, 
						 psOutputColour,
						 psLightSource,
						 psLightProduct,
						 psLtCoeffs,
						 1);

	if (bSpecularPresent)
	{
		FFTNLAddSpecularLight(psFFGenCode,
							  ui32FFTNLEnables1,
							  psInputColour, 
							  psOutputColour,
							  psOutputSecColour, 
							  psLightSource,
							  psLightProduct,
							  psLtCoeffs,
							  2);
	}
		
	ReleaseReg(psFFGenCode, psLtCoeffs);
	psLtCoeffs = IMG_NULL;

#ifdef FFGEN_UNIFLEX
	if(IF_FFGENCODE_UNIFLEX)
	{
		if (bSpecularPresent)
			END_PRED(psFFGenCode, IMG_NULL);
	}
	else
#endif
	{
		/* Insert label marking end of this lights code */
		psLabel->uOffset = uEndOfLightLabel;
		INST0(LABEL, psLabel, "End of light");
	}

	/* Release temp register */
	ReleaseReg(psFFGenCode, psTemp1);
	psTemp1 = IMG_NULL;
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
static IMG_VOID FFTNLGenSpotInfiniteLighting(FFGenCode *psFFGenCode,
									  IMG_UINT32  ui32FFTNLEnables1,
									  FFGenReg     *psOutputSecColour,
									  FFGenReg     *psInputColour,
									  FFGenReg     *psOutputColour,
									  FFGenReg     *psCurrentColour,
									  FFGenReg     *psMaterial,
									  FFGenReg     *psLabel,
									  FFGenReg     *psLightSource,
									  FFGenReg     *psLightProduct,
									  IMG_BOOL      bSpecularPresent)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

	FFGenReg *psNormal = psFFGenCode->psNormal;

	IMG_BOOL bNormaliseSpecHalfAngle = IMG_FALSE;

	FFGenReg  *psVtxLitVec     = IMG_NULL;
	FFGenReg  *psSpecHalfAngle = IMG_NULL;

	FFGenReg *psTemp1,     *psVtxLitVecMagSq,  *psSpotAttenProd;
	FFGenReg *psDiffAngle, *psLtCoeffs, *psSpotlight;

	FFGenReg *psVtxEyeVec = psFFGenCode->psVtxEyeVec;

	FFGenReg *psImmediateFloatReg  = &(psFFGenCode->sImmediateFloatReg);

	/* Get predicate regs */
	FFGenReg *psPredReg0        = &(psFFGenCode->asPredicateRegs[0]);

	/* 
	   Get a label which we'll place at the end of the lighting code so we
	   can skip to it at any time 
	*/
	IMG_UINT32 uEndOfLightLabel = GetLabel(psFFGenCode, "EndOfLight");

	psTemp1 = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 4, IMG_NULL);

	/* get temp for vertex to light vector */
	psVtxLitVec = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

	/* get temp for vertex to light vector magnitude squared */
	psVtxLitVecMagSq = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* fdp3			r[DP3_DST(tVtxLitVecMagSq)],			c[cLt0Pos],				c[cLt0Pos] ; */
	SETDESTOFF(DP3_ADJUST(0));
	SETSRCOFF(0, FFTNL_OFFSETS_LIGHTSOURCE_POSITION); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_POSITION);
	INST2(FDP3, psVtxLitVecMagSq, psLightSource, psLightSource, 
		  "Normalise vertex to light vector");

	/*
	  frsq			r[tT1],				     	r[tVtxLitVecMagSq] ;
	  fmul			r[tVtxLitVec + _X_],		r[tT1],				 c[cLt0Pos + _X_] ;
	  fmul			r[tVtxLitVec + _Y_],		r[tT1],				 c[cLt0Pos + _Y_] ;
	  fmul			r[tVtxLitVec + _Z_],		r[tT1],				 c[cLt0Pos + _Z_] ;
	*/

	INST1(FRSQ, psTemp1, psVtxLitVecMagSq, IMG_NULL);

	SETDESTOFF(0);  SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_POSITION + 0);
	INST2(FMUL, psVtxLitVec, psTemp1, psLightSource,  IMG_NULL);

	SETDESTOFF(1);  SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_POSITION + 1);
	INST2(FMUL, psVtxLitVec, psTemp1, psLightSource, IMG_NULL);

	SETDESTOFF(2);  SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_POSITION + 2);
	INST2(FMUL, psVtxLitVec, psTemp1, psLightSource, IMG_NULL);

	/* Get spotlight register */
	psSpotlight = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* fdp3			r[DP3_DST(tSpotlight)],		-r[tVtxLitVec],		r[cLt0Spot]; */
	SETSRCFLAG(0, USEASM_ARGFLAGS_NEGATE);
	SETDESTOFF(DP3_ADJUST(0)); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_SPOTDIRECTION);
	INST2(FDP3, psSpotlight, psVtxLitVec, psLightSource, "Calculate angle between light to vertex vector and spotlight direction (spot_i)");

	/* fsub.testn		p0,							r[tSpotlight],		r[cLt0SpotCutOffCos] */
	SET_TESTPRED_NEGATIVE();
	SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_SPOTCOSCUTOFF);
	INST2(FSUB, psPredReg0, psSpotlight, psLightSource, "If vertex falls outside of (cosine) of spotlight cutoff then it is not lit");

	/* Check if light is outside spotlight cut off */

	/* p0 br EndOfLight */
	psLabel->uOffset = uEndOfLightLabel;
	USE_PRED(psPredReg0);
	INST0(BR, psLabel, "Move onto next light");

	/*
	  flog			r[tSpotlight],				r[tSpotlight] ;
	  fmul			r[tSpotlight],				r[tSpotlight],				c[cSpotExponent] ; 
	  fexp			r[tSpotlight],				r[tSpotlight] ; 
	*/
	INST1(FLOG, psSpotlight, psSpotlight, "Calculate attenuation due to spotlight falloff");

	SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_SPOTEXPONENT);
	INST2(FMUL, psSpotlight, psSpotlight, psLightSource, IMG_NULL);

	INST1(FEXP, psSpotlight, psSpotlight, IMG_NULL);

	/* Release reg for vertex lit vec magnitude squared */
	ReleaseReg(psFFGenCode, psVtxLitVecMagSq);
	psVtxLitVecMagSq = IMG_NULL;

	/* For infinite spot lights the spotlight is the only attenuation that is calculated */
	psSpotAttenProd = psSpotlight;

	/* Get temp for diffuse angle calculation */
	psDiffAngle = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* fdp3			r[DP3_DST(tDiffAngle)],		r[tVtxLitVec],		r[tNormal];        // f_i (n dot VP_pli) */
	SETDESTOFF(DP3_ADJUST(0)); 
	INST2(FDP3, psDiffAngle, psVtxLitVec, psNormal, "Calculate angle between normal and vertex to light vector");

	if (bSpecularPresent)
	{
		/* fmov.testn|z	p0,							r[tDiffAngle] ; */
		SET_TESTPRED_NEGATIVE_OR_ZERO();
		INST1(FMOV, psPredReg0, psDiffAngle, "Test result - !! shift this test into prev instruction when HW and compiler fixed to support this !!");

		IF_PRED(psFFGenCode, psPredReg0, "AmbientOnly", "Check if it's worth calculating the diffuse and specular components");

		FFTNLAddAmbientLight(psFFGenCode,
							 ui32FFTNLEnables1,
							 psInputColour,
							 psOutputColour,
							 psOutputSecColour,
							 psCurrentColour,
							 psLightSource,
							 psLightProduct,
							 psSpotAttenProd,
							 0);

		/* br uEndOfLightLabel: */
		psLabel->uOffset = uEndOfLightLabel;
		INST0(BR, psLabel, "Move onto next light");

		END_PRED(psFFGenCode, IMG_NULL);
	}
	else
	{
		SET_FLOAT_ZERO(psImmediateFloatReg);
		INST2(FMAX, psDiffAngle, psDiffAngle, psImmediateFloatReg, "Clamp diff angle to 0.0");
	}

	if (bSpecularPresent)
	{
		psSpecHalfAngle = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_LOCALVIEWER)
		{
			/* fadd.rpt3	r[tSpecHalfAngle],				r[tVtxLitVec],					r[tVtxEyeVec] */
			SET_REPEAT_COUNT(3);
			INST2(FADD, psSpecHalfAngle, psVtxLitVec, psVtxEyeVec, "Local viewer enabled : h_i = (VP_pli + VP_e)");
			
			bNormaliseSpecHalfAngle = IMG_TRUE;
		}
		else
		{
			/* This can be optimised out */
			SET_REPEAT_COUNT(3);
			SETSRCOFF(0, FFTNL_OFFSETS_LIGHTSOURCE_HALFVECTOR); 
			INST1(MOV, psSpecHalfAngle, psLightSource, "Load light half angle");
			
			bNormaliseSpecHalfAngle = IMG_FALSE;
		}
	}

	/* Release the vertex lit vector */
	ReleaseReg(psFFGenCode, psVtxLitVec);
	psVtxLitVec = IMG_NULL;

	if (bSpecularPresent)
	{
		if (bNormaliseSpecHalfAngle)
		{
			/*
			  fdp3		r[DP3_DST(tT1)],				r[tSpecHalfAngle],				r[tSpecHalfAngle];
			  frsq		r[tT1],							r[tT1];
			  fmul		r[tSpecHalfAngle + _X_],		r[tSpecHalfAngle + _X_],		r[tT1];
			  fmul		r[tSpecHalfAngle + _Y_],		r[tSpecHalfAngle + _Y_],		r[tT1];
			  fmul		r[tSpecHalfAngle + _Z_],		r[tSpecHalfAngle + _Z_],		r[tT1];
			*/

			SETDESTOFF(DP3_ADJUST(0)); 
			INST2(FDP3, psTemp1, psSpecHalfAngle, psSpecHalfAngle, "Normalise specular half angle");

			INST1(FRSQ, psTemp1, psTemp1, IMG_NULL);

			SETDESTOFF(0); SETSRCOFF(0, 0);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);

			SETDESTOFF(1); SETSRCOFF(0, 1);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);

			SETDESTOFF(2); SETSRCOFF(0, 2);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);
		}


		/* 	fdp3		r[DP3_DST(tSpecHalfAngle)],		r[tNormal],						r[tSpecHalfAngle]; */
		SETDESTOFF(DP3_ADJUST(0)); 
		INST2(FDP3, psSpecHalfAngle, psNormal, psSpecHalfAngle, "Calc specular half angle ");

		/* fmov.testp|z	p0,							r[tSpecHalfAngle] ; */
		SET_TESTPRED_POSITIVE_OR_ZERO();
		INST1(FMOV, psPredReg0, psSpecHalfAngle, "Set predicate for specular test - MOVE INTO ABOVE INSTRUCTION WHEN SUPPORTED");

		IF_PRED(psFFGenCode, psPredReg0, "SpecLightCoeff", "Do we need to calc specular coeff?");

		/*
		  flog		r[tSpecHalfAngle],				r[tSpecHalfAngle] ;
		  fmul		r[tSpecHalfAngle],				r[tSpecHalfAngle],				c[cMatShin] ; 
		  fexp		r[tSpecHalfAngle],				r[tSpecHalfAngle] ; 
		*/

		INST1(FLOG, psSpecHalfAngle, psSpecHalfAngle, "Calculate specular coeff");

		SETSRCOFF(1, FFTNL_OFFSETS_MATERIAL_SHININESS);
		INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psMaterial, IMG_NULL);

		INST1(FEXP, psSpecHalfAngle, psSpecHalfAngle, IMG_NULL);

		ELSE_PRED(psFFGenCode, "NoSpecLightCoeff", IMG_NULL);

		/* 	mov			r[tSpecHalfAngle],				#0.0 ; */
		SET_FLOAT_ZERO(psImmediateFloatReg);
		INST1(MOV, psSpecHalfAngle, psImmediateFloatReg, "Set specular coeff to 0");

		END_PRED(psFFGenCode, IMG_NULL);
	}

	psLtCoeffs = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 4, IMG_NULL);

	/*
	  fmul		r[tLtCoeffs + _X_],				#1.0                    r[tSpotAttenProd];
	  fmul		r[tLtCoeffs + _Y_],				r[tDiffangle],   		r[tSpotAttenProd];
	  fmul		r[tLtCoeffs + _Z_],				r[tSpecHalfAngle],		r[tSpotAttenProd];
	*/
	SETDESTOFF(0);
	INST1(MOV, psLtCoeffs, psSpotAttenProd, "Attenuate light coefficients");

	SETDESTOFF(1);
	INST2(FMUL, psLtCoeffs, psDiffAngle, psSpotAttenProd, IMG_NULL);


	if (bSpecularPresent)
	{
		SETDESTOFF(2); 
		INST2(FMUL, psLtCoeffs, psSpecHalfAngle, psSpotAttenProd, IMG_NULL);
	}

	/* Release diffuse angle reg */
	ReleaseReg(psFFGenCode, psDiffAngle);
	psDiffAngle = IMG_NULL;

	if (bSpecularPresent)
	{
		/* Release specular half angle */
		ReleaseReg(psFFGenCode, psSpecHalfAngle);
		psSpecHalfAngle = IMG_NULL;
	}

	/* Release spot attenuation product */
	ReleaseReg(psFFGenCode, psSpotAttenProd);
	psSpotAttenProd = IMG_NULL;

	FFTNLAddAmbientLight(psFFGenCode,
						 ui32FFTNLEnables1,
						 psInputColour, 
						 psOutputColour,
						 psOutputSecColour,
						 psCurrentColour,
						 psLightSource,
						 psLightProduct,
						 psLtCoeffs,
						 0);

	FFTNLAddDiffuseLight(psFFGenCode,
						 ui32FFTNLEnables1,
						 psInputColour, 
						 psOutputColour,
						 psOutputSecColour, 
						 psOutputColour,
						 psLightSource,
						 psLightProduct,
						 psLtCoeffs,
						 1);

	if (bSpecularPresent)
	{
		FFTNLAddSpecularLight(psFFGenCode,
							  ui32FFTNLEnables1,
							  psInputColour, 
							  psOutputColour,
							  psOutputSecColour, 
							  psLightSource,
							  psLightProduct,
							  psLtCoeffs,
							  2);
	}

	ReleaseReg(psFFGenCode, psLtCoeffs);
	psLtCoeffs = IMG_NULL;

	/* Insert label marking end of this lights code */
	psLabel->uOffset = uEndOfLightLabel;
	INST0(LABEL, psLabel, "End of light");

	/* Release temp register */
	ReleaseReg(psFFGenCode, psTemp1);
	psTemp1 = IMG_NULL;
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
static IMG_VOID FFTNLGenSpotLocalLighting(FFGenCode *psFFGenCode,
								   IMG_UINT32  ui32FFTNLEnables1,
								   FFGenReg     *psOutputSecColour,
								   FFGenReg     *psInputColour,
								   FFGenReg     *psOutputColour,
								   FFGenReg     *psCurrentColour,
								   FFGenReg     *psMaterial,
								   FFGenReg     *psLabel,
								   FFGenReg     *psLightSource,
								   FFGenReg     *psLightProduct,
								   IMG_BOOL      bSpecularPresent)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

	FFGenReg *psNormal = psFFGenCode->psNormal;

	FFGenReg *psEyePos = psFFGenCode->psEyePosition;

	IMG_BOOL bNormaliseSpecHalfAngle = IMG_FALSE;

	FFGenReg  *psVtxLitVec     = IMG_NULL;
	FFGenReg  *psSpecHalfAngle = IMG_NULL;
	FFGenReg  *psDistVec = IMG_NULL; //
	FFGenReg  *psDistAtten = IMG_NULL; //

	FFGenReg *psTemp1,     *psVtxLitVecMagSq,  *psSpotAttenProd;
	FFGenReg *psDiffAngle, *psLtCoeffs, *psSpotlight;

	FFGenReg *psVtxEyeVec = psFFGenCode->psVtxEyeVec;

	FFGenReg *psImmediateFloatReg  = &(psFFGenCode->sImmediateFloatReg);

	/* Get predicate regs */
	FFGenReg *psPredReg0        = &(psFFGenCode->asPredicateRegs[0]);

	/* 
	   Get a label which we'll place at the end of the lighting code so we
	   can skip to it at any time 
	*/
	IMG_UINT32 uEndOfLightLabel = GetLabel(psFFGenCode, "EndOfLight"); 

	psTemp1 = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 4, IMG_NULL);

	/* get temp for vertex to light vector */
	psVtxLitVec = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

	/* get temp for vertex to light vector magnttude squared */
	psVtxLitVecMagSq = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* 	//fsub.rpt3		r[tVtxLitVec],				c[cLt0Pos],				r[tEyePos]; */
	/* 	fadd.rpt3		r[tVtxLitVec],				-r[tEyePos];            c[cLt0Pos] */
	SET_REPEAT_COUNT(3);
	SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_POSITION);
	SETSRCFLAG(0, USEASM_ARGFLAGS_NEGATE);
	INST2(FADD, psVtxLitVec, psEyePos, psLightSource, "Get vertex to light vector (fsub not support sa as 1st src so changed to add)");


	/* fdp3			r[DP3_DST(tVtxLitVecMagSq)],		r[tVtxLitVec],				r[tVtxLitVec] ;	*/
	SETDESTOFF(DP3_ADJUST(0));
	INST2(FDP3, psVtxLitVecMagSq, psVtxLitVec, psVtxLitVec, "Normalise vertex to light vector");

	/*
	  frsq			r[tT1],				     	r[tVtxLitVecMagSq] ;
	  fmul			r[tVtxLitVec + _X_],		r[tT1],				r[tVtxLitVec + _X_] ;
	  fmul			r[tVtxLitVec + _Y_],		r[tT1],				r[tVtxLitVec + _Y_] ;
	  fmul			r[tVtxLitVec + _Z_],		r[tT1],				r[tVtxLitVec + _Z_] ;
	*/

	INST1(FRSQ, psTemp1, psVtxLitVecMagSq, IMG_NULL);

	SETDESTOFF(0);  SETSRCOFF(1, 0); 
	INST2(FMUL, psVtxLitVec, psTemp1, psVtxLitVec, IMG_NULL);

	SETDESTOFF(1);  SETSRCOFF(1, 1); 
	INST2(FMUL, psVtxLitVec, psTemp1, psVtxLitVec, IMG_NULL);

	SETDESTOFF(2);  SETSRCOFF(1, 2); 
	INST2(FMUL, psVtxLitVec, psTemp1, psVtxLitVec, IMG_NULL);


	/* Get spotlight register */
	psSpotlight = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* fdp3			r[DP3_DST(tSpotlight)],		-r[tVtxLitVec],		r[cLt0Spot]; */
	SETSRCFLAG(0, USEASM_ARGFLAGS_NEGATE);
	SETDESTOFF(DP3_ADJUST(0)); SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_SPOTDIRECTION);
	INST2(FDP3, psSpotlight, psVtxLitVec, psLightSource, "Calculate angle between light to vertex vector and spotlight direction (spot_i)");

	/* Check if light is outside spotlight cut off */
	/* fsub.testn		p0,							r[tSpotlight],		r[cLt0SpotCutOffCos] */
	SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_SPOTCOSCUTOFF);

#ifdef FFGEN_UNIFLEX
	if(IF_FFGENCODE_UNIFLEX)
	{
		SET_TESTPRED_POSITIVE_OR_ZERO();
		INST2(FSUB, psPredReg0, psSpotlight, psLightSource, IMG_NULL);

		IF_PRED(psFFGenCode, psPredReg0, IMG_NULL, IMG_NULL);
	}
	else
#endif
	{
		SET_TESTPRED_NEGATIVE();
		INST2(FSUB, psPredReg0, psSpotlight, psLightSource, "If vertex falls outside of (cosine) of spotlight cutoff then it is not lit");

		/* p0 br EndOfLight */
		psLabel->uOffset = uEndOfLightLabel;
		USE_PRED(psPredReg0);
		INST0(BR, psLabel, "Move onto next light");
	}

	/*
	  flog			r[tSpotlight],				r[tSpotlight] ;
	  fmul			r[tSpotlight],				r[tSpotlight],				c[cSpotExponent] ; 
	  fexp			r[tSpotlight],				r[tSpotlight] ; 
	*/
	INST1(FLOG, psSpotlight, psSpotlight, "Calculate attenuation due to spotlight falloff");

	SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_SPOTEXPONENT);
	INST2(FMUL, psSpotlight, psSpotlight, psLightSource, IMG_NULL);

	INST1(FEXP, psSpotlight, psSpotlight, IMG_NULL);


	/* get reg for distance vector */
	psDistVec = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

	/*
	  mov			r[tDistVec + _X_],			#1.0 ;
	  fmul			r[tDistVec + _Y_],			r[tVtxLitVecMagSq],				r[tT1] ;
	  fmov			r[tDistVec + _Z_],			r[tVtxLitVecMagSq] ;
	*/

	SET_FLOAT_ONE(psImmediateFloatReg);
	SETDESTOFF(0);
	INST1(MOV, psDistVec, psImmediateFloatReg, "Setup distance vector  = {1, |VP_pli|, SQ(|VP_pli|)");

	SETDESTOFF(1);
	INST2(FMUL, psDistVec, psVtxLitVecMagSq, psTemp1, IMG_NULL);

	SETDESTOFF(2);
	INST1(FMOV, psDistVec, psVtxLitVecMagSq, IMG_NULL);

	/* Get distance attenuation register */
	psDistAtten = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* fdp3	r[tDistAtten],		r[tDistVec],		c[cLt0Atten]; */
	SETDESTOFF(DP3_ADJUST(0)); 
	SETSRCOFF(1, FFTNL_OFFSETS_LIGHTSOURCE_CONSTANTATTENUATION);
	INST2(FDP3, psDistAtten, psDistVec, psLightSource, "Calculate distance attenuation");

	/* Relase distance vector */
	ReleaseReg(psFFGenCode, psDistVec);
	psDistVec = IMG_NULL;

	/* frcp			r[tDistAtten],				r[tDistAtten];         // (att_i); */
	INST1(FRCP, psDistAtten, psDistAtten, "(att_i)");


	/* Release reg for vertex lit vec magnitude squared */
	ReleaseReg(psFFGenCode, psVtxLitVecMagSq);
	psVtxLitVecMagSq = IMG_NULL;


	psSpotAttenProd = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* fmul			r[tSpotAttenProd],			r[tDistAtten],				r[tSpotlight]; */
	INST2(FMUL, psSpotAttenProd, psDistAtten, psSpotlight, "Add spotlight calc to distance attenuation");

	/* Release spotlight and attenuation registers */
	ReleaseReg(psFFGenCode, psDistAtten);
	psDistAtten = IMG_NULL;

	ReleaseReg(psFFGenCode, psSpotlight);
	psSpotlight = IMG_NULL;

	/* Get temp for diffuse angle calculation */
	psDiffAngle = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 1, IMG_NULL);

	/* fdp3			r[DP3_DST(tDiffAngle)],		r[tVtxLitVec],		r[tNormal];        // f_i (n dot VP_pli) */
	SETDESTOFF(DP3_ADJUST(0)); 
	INST2(FDP3, psDiffAngle, psVtxLitVec, psNormal, "Calculate angle between normal and vertex to light vector");

	if (bSpecularPresent)
	{
		/* fmov.testn|z	p0,							r[tDiffAngle] ; */
		SET_TESTPRED_NEGATIVE_OR_ZERO();
		INST1(FMOV, psPredReg0, psDiffAngle, "Test result - !! shift this test into prev instruction when HW and compiler fixed to support this !!");

		IF_PRED(psFFGenCode, psPredReg0, "AmbientOnly", "Check if it's worth calculating the diffuse and specular components");

		FFTNLAddAmbientLight(psFFGenCode,
							 ui32FFTNLEnables1,
							 psInputColour,
							 psOutputColour,
							 psOutputSecColour,
							 psCurrentColour,
							 psLightSource,
							 psLightProduct,
							 psSpotAttenProd,
							 0);

#ifdef FFGEN_UNIFLEX
		if(IF_FFGENCODE_UNIFLEX)
		{
			ELSE_PRED(psFFGenCode, IMG_NULL, IMG_NULL);
		}
		else
#endif
		{
			/* br uEndOfLightLabel: */
			psLabel->uOffset = uEndOfLightLabel;
			INST0(BR, psLabel, "Move onto next light");

			END_PRED(psFFGenCode, IMG_NULL);
		}
	}
	else
	{
		SET_FLOAT_ZERO(psImmediateFloatReg);
		INST2(FMAX, psDiffAngle, psDiffAngle, psImmediateFloatReg, "Clamp diff angle to 0.0");
	}

	if (bSpecularPresent)
	{
		psSpecHalfAngle = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 3, IMG_NULL);

		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_LOCALVIEWER)
		{
			/* fadd.rpt3	r[tSpecHalfAngle],				r[tVtxLitVec],					r[tVtxEyeVec] */
			SET_REPEAT_COUNT(3);
			INST2(FADD, psSpecHalfAngle, psVtxLitVec, psVtxEyeVec, "Local viewer enabled : h_i = (VP_pli + VP_e)");

			bNormaliseSpecHalfAngle = IMG_TRUE;
		}
		else
		{
			/* fadd.rpt3	r[tSpecHalfAngle],				r[tVtxLitVec],					c[cLitVec001] */
			SET_FLOAT_ZERO(psImmediateFloatReg);
			SETDESTOFF(0); SETSRCOFF(0, 0); 
			INST2(FADD, psSpecHalfAngle, psVtxLitVec, psImmediateFloatReg, "Local viewer disabled :h_i = (VP_pli + {0,0,1})");

			SETDESTOFF(1); SETSRCOFF(0, 1); 
			INST2(FADD, psSpecHalfAngle, psVtxLitVec, psImmediateFloatReg, IMG_NULL);

			SET_FLOAT_ONE(psImmediateFloatReg);
			SETDESTOFF(2); SETSRCOFF(0, 2); 
			INST2(FADD, psSpecHalfAngle, psVtxLitVec, psImmediateFloatReg, IMG_NULL);

			bNormaliseSpecHalfAngle = IMG_TRUE;
		}
	}


	/* Release the vertex lit vector */
	ReleaseReg(psFFGenCode, psVtxLitVec);
	psVtxLitVec = IMG_NULL;

	if (bSpecularPresent)
	{
		if (bNormaliseSpecHalfAngle)
		{
			/*
			  fdp3		r[DP3_DST(tT1)],				r[tSpecHalfAngle],				r[tSpecHalfAngle];
			  frsq		r[tT1],							r[tT1];
			  fmul		r[tSpecHalfAngle + _X_],		r[tSpecHalfAngle + _X_],		r[tT1];
			  fmul		r[tSpecHalfAngle + _Y_],		r[tSpecHalfAngle + _Y_],		r[tT1];
			  fmul		r[tSpecHalfAngle + _Z_],		r[tSpecHalfAngle + _Z_],		r[tT1];
			*/

			SETDESTOFF(DP3_ADJUST(0)); 
			INST2(FDP3, psTemp1, psSpecHalfAngle, psSpecHalfAngle, "Normalise specular half angle");

			INST1(FRSQ, psTemp1, psTemp1, IMG_NULL);

			SETDESTOFF(0); SETSRCOFF(0, 0);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);

			SETDESTOFF(1); SETSRCOFF(0, 1);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);

			SETDESTOFF(2); SETSRCOFF(0, 2);
			INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psTemp1, IMG_NULL);
		}


		/* 	fdp3		r[DP3_DST(tSpecHalfAngle)],		r[tNormal],						r[tSpecHalfAngle]; */
		SETDESTOFF(DP3_ADJUST(0)); 
		INST2(FDP3, psSpecHalfAngle, psNormal, psSpecHalfAngle, "Calc specular half angle ");

		/* fmov.testp|z	p0,							r[tSpecHalfAngle] ; */
		SET_TESTPRED_POSITIVE_OR_ZERO();
		INST1(FMOV, psPredReg0, psSpecHalfAngle, "Set predicate for specular test - MOVE INTO ABOVE INSTRUCTION WHEN SUPPORTED");

		IF_PRED(psFFGenCode, psPredReg0, "SpecLightCoeff", "Do we need to calc specular coeff?");

		/*
		  flog		r[tSpecHalfAngle],				r[tSpecHalfAngle] ;
		  fmul		r[tSpecHalfAngle],				r[tSpecHalfAngle],				c[cMatShin] ; 
		  fexp		r[tSpecHalfAngle],				r[tSpecHalfAngle] ; 
		*/

		INST1(FLOG, psSpecHalfAngle, psSpecHalfAngle, "Calculate specular coeff");

		SETSRCOFF(1, FFTNL_OFFSETS_MATERIAL_SHININESS);
		INST2(FMUL, psSpecHalfAngle, psSpecHalfAngle, psMaterial, IMG_NULL);

		INST1(FEXP, psSpecHalfAngle, psSpecHalfAngle, IMG_NULL);

		ELSE_PRED(psFFGenCode, "NoSpecLightCoeff", IMG_NULL);

		/* 	mov			r[tSpecHalfAngle],				#0.0 ; */
		SET_FLOAT_ZERO(psImmediateFloatReg);
		INST1(MOV, psSpecHalfAngle, psImmediateFloatReg, "Set specular coeff to 0");

		END_PRED(psFFGenCode, IMG_NULL);
	}

	psLtCoeffs = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, 4, IMG_NULL);

	/*
	  fmul		r[tLtCoeffs + _X_],				#1.0                    r[tSpotAttenProd];
	  fmul		r[tLtCoeffs + _Y_],				r[tDiffangle],   		r[tSpotAttenProd];
	  fmul		r[tLtCoeffs + _Z_],				r[tSpecHalfAngle],		r[tSpotAttenProd];
	*/
	SETDESTOFF(0); 
	INST1(MOV, psLtCoeffs, psSpotAttenProd, "Attenuate light coefficients");

	SETDESTOFF(1); 
	INST2(FMUL, psLtCoeffs, psDiffAngle, psSpotAttenProd, IMG_NULL);

	if (bSpecularPresent)
	{
		SETDESTOFF(2); 
		INST2(FMUL, psLtCoeffs, psSpecHalfAngle, psSpotAttenProd, IMG_NULL);
	}

	/* Release diffuse angle reg */
	ReleaseReg(psFFGenCode, psDiffAngle);
	psDiffAngle = IMG_NULL;


	if (bSpecularPresent)
	{
		/* Release specular half angle */
		ReleaseReg(psFFGenCode, psSpecHalfAngle);
		psSpecHalfAngle = IMG_NULL;
	}

	/* Release spot attenuation product */
	ReleaseReg(psFFGenCode, psSpotAttenProd);
	psSpotAttenProd = IMG_NULL;

	FFTNLAddAmbientLight(psFFGenCode,
						 ui32FFTNLEnables1,
						 psInputColour,
						 psOutputColour,
						 psOutputSecColour,
						 psCurrentColour,
						 psLightSource,
						 psLightProduct,
						 psLtCoeffs,
						 0);

	FFTNLAddDiffuseLight(psFFGenCode,
						 ui32FFTNLEnables1,
						 psInputColour, 
						 psOutputColour,
						 psOutputSecColour,
						 psOutputColour,
						 psLightSource,
						 psLightProduct,
						 psLtCoeffs,
						 1);

	if (bSpecularPresent)
	{
		FFTNLAddSpecularLight(psFFGenCode,
							  ui32FFTNLEnables1,
							  psInputColour,
							  psOutputColour,
							  psOutputSecColour,
							  psLightSource,
							  psLightProduct,
							  psLtCoeffs,
							  2);
	}

	ReleaseReg(psFFGenCode, psLtCoeffs);
	psLtCoeffs = IMG_NULL;

#ifdef FFGEN_UNIFLEX
	if(IF_FFGENCODE_UNIFLEX)
	{
		if (bSpecularPresent)
			END_PRED(psFFGenCode, IMG_NULL);

		END_PRED(psFFGenCode, IMG_NULL);
	}
	else
#endif
	{
		/* Insert label marking end of this lights code */
		psLabel->uOffset = uEndOfLightLabel;
		INST0(LABEL, psLabel, "End of light");
	}

	/* Release temp register */
	ReleaseReg(psFFGenCode, psTemp1);
	psTemp1 = IMG_NULL;
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
IMG_INTERNAL IMG_VOID FFTNLGenOptimisedLighting(FFGenCode *psFFGenCode)
{
	/* 
	   NOTE ABOUT TWO SIDED LIGHTING 
	   Current implementation is not most efficient if light constants not stored in sec attribs.
	   In this case it would be faster to store a copy of the inverted normal and do front-face/back-face
	   for each light in turn as apposed doing all front face for all lights then the back face. This
	   is because the loading of the light source info from memory becomes high cost as it is done
	   twice for every light.
	   Two sided lighting rarely used so probably not worth the time to do this though.
	*/

	/* Get the FF description */
	FFTNLGenDesc *psFFTNLGenDesc            = psFFGenCode->psFFTNLGenDesc;
	
	/* Get the lighting enables */
	IMG_UINT32              ui32FFTNLEnables1 = psFFTNLGenDesc->ui32FFTNLEnables1 & (FFTNL_ENABLES1_LIGHTINGENABLED    |
																			   FFTNL_ENABLES1_LIGHTINGTWOSIDED   |
																			   FFTNL_ENABLES1_LOCALVIEWER        |
																			   FFTNL_ENABLES1_SEPARATESPECULAR   |
																			   FFTNL_ENABLES1_COLOURMATERIAL     |
																			   FFTNL_ENABLES1_VERTCOLOUREMISSIVE |
																			   FFTNL_ENABLES1_VERTCOLOURAMBIENT  |
																			   FFTNL_ENABLES1_VERTCOLOURDIFFUSE  |
																			   FFTNL_ENABLES1_VERTCOLOURSPECULAR |
																			   FFTNL_ENABLES1_TRANSFORMNORMALS);

	/* Get almalgamation of all enabled lights */
	IMG_UINT32  uEnabledLights = (psFFTNLGenDesc->uEnabledSpotLocalLights     |
								  psFFTNLGenDesc->uEnabledSpotInfiniteLights  |
								  psFFTNLGenDesc->uEnabledPointLocalLights    |
								  psFFTNLGenDesc->uEnabledPointInfiniteLights);

	/* Is two sided lighting enabled? */
	IMG_BOOL bTwoSidedLighting = (ui32FFTNLEnables1 & FFTNL_ENABLES1_LIGHTINGTWOSIDED) ? IMG_TRUE : IMG_FALSE; 

	if (ui32FFTNLEnables1 & FFTNL_ENABLES1_LIGHTINGENABLED)
	{

		IMG_UINT32 uLightCount = 0;
		IMG_UINT32 uNumEnabledLights = 0;

		IMG_BOOL   bFirstLightIsSpot = IMG_FALSE;

		IMG_UINT32 uBackFaceIndex = 0;

		FFGenReg *psOutputSecColour, *psInputColour, *psOutputColour, *psLightProduct, *psMaterial, *psCurrentColour;
		FFGenReg *psLightModelProduct  = IMG_NULL;

		IMG_UINT32 uCopySize;

		FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

		FFGenReg *psLabel = &(psFFGenCode->sLabelReg);

		FFGenReg *psNormal = psFFGenCode->psNormal;

		FFGenReg *psImmediateFloatReg  = &(psFFGenCode->sImmediateFloatReg);

		/* Get the input colour */
		psInputColour = GetReg(psFFGenCode, USEASM_REGTYPE_PRIMATTR, FFGEN_INPUT_COLOR, 0, 4, IMG_NULL);

 StartOfLighting:

		NEW_BLOCK(psFFGenCode, uBackFaceIndex ? "Lighting back face" : "Lighting front face");

		/* Get the output colour */
		psOutputColour = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTCOLOR + uBackFaceIndex, 0, 4, IMG_NULL);

		/* Setup an initial colour to do the first accumulate with */
		psCurrentColour = psOutputColour;
		
		/* Decide where the specular writes should go */
		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_SEPARATESPECULAR)
		{
			psOutputSecColour = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTSECONDARYCOLOR + uBackFaceIndex, 0, 4, IMG_NULL);

			/* 
			   Non optimal fix - best way is to do mul on 1st colour accumulate then mads 
			   Do something like psCurrentSecColour
			
			   Zero reg 
			*/
			SET_FLOAT_ZERO(psImmediateFloatReg);
			SET_REPEAT_COUNT(3);
			INST1(MOV, psOutputSecColour, psImmediateFloatReg, "Zero seperate specular reg (non optimal)");
		}
		else
		{
			psOutputSecColour = psOutputColour;
		}

		/* Count number of lights */
		while (uEnabledLights)
		{
			if (uEnabledLights & 0x1)
			{
				if (!uNumEnabledLights)
				{
					/* Was first enabled light a spotlight? */
					if ((psFFTNLGenDesc->uEnabledSpotLocalLights     & (0x1 << uLightCount)) ||
						(psFFTNLGenDesc->uEnabledSpotInfiniteLights  & (0x1 << uLightCount)))
					{
						bFirstLightIsSpot = IMG_TRUE;
					}
				}

				uNumEnabledLights++;
			}

			/* Actually the maximum light number that is enabled */
			uLightCount++;
			
			uEnabledLights >>= 1;
		}

		if (uLightCount)
		{
			if (uBackFaceIndex == 0 || !bTwoSidedLighting)
			{
				IMG_UINT32 i;

				uEnabledLights = (psFFTNLGenDesc->uEnabledSpotLocalLights     |
								  psFFTNLGenDesc->uEnabledSpotInfiniteLights  |
								  psFFTNLGenDesc->uEnabledPointLocalLights    |
								  psFFTNLGenDesc->uEnabledPointInfiniteLights);

				for(i=0; i<uLightCount; i++)
				{
					/* Only allocate space for the enabled lights */
					if(uEnabledLights & (0x1 << i))
					{
						AllocRegSpace(psFFGenCode,
									  USEASM_REGTYPE_SECATTR, 
									  FFGEN_STATE_LIGHTSOURCE0 + i, 
									  FFTNL_SIZE_LIGHTSOURCE, 
									  IMG_FALSE);
					}
				}
			}
			
			/* Allocate reg space for the light products */
			AllocRegSpace(psFFGenCode, 
						  USEASM_REGTYPE_SECATTR, 
						  FFGEN_STATE_FRONTLIGHTPRODUCT + uBackFaceIndex, 
						  uLightCount * FFTNL_SIZE_LIGHTPRODUCT, 
						  IMG_FALSE);
		}

		/* Always copy 3 values, set the alpha explicitly later */
		uCopySize = 3;

		/* Load the material */
		psMaterial = GetReg(psFFGenCode,
							USEASM_REGTYPE_SECATTR,
							FFGEN_STATE_FRONTMATERIAL + uBackFaceIndex,
							0,
							FFTNL_SIZE_MATERIAL,
							"Material");

		if (ui32FFTNLEnables1 & (FFTNL_ENABLES1_VERTCOLOUREMISSIVE | FFTNL_ENABLES1_VERTCOLOURAMBIENT))
		{
			FFGenReg *psLightModel = GetReg(psFFGenCode,
											USEASM_REGTYPE_SECATTR,
											FFGEN_STATE_LIGHTMODEL,
											0,
											4,
											"Light Model");

			if (ui32FFTNLEnables1 & FFTNL_ENABLES1_VERTCOLOUREMISSIVE)
			{
				/* fmad	o[oPrimCol].rpt3,		c[cLMSceneAmbi],		c[cMatAmbi],	pa[vPrimCol]; */
				SET_REPEAT_COUNT(uCopySize);
				SETSRCOFF(1, FFTNL_OFFSETS_MATERIAL_AMBIENT);
				INST3(FMAD, psOutputColour, psLightModel, psMaterial, psInputColour, "Scene colour - Use vertex colour for material emissive");
			}
			else if (ui32FFTNLEnables1 & FFTNL_ENABLES1_VERTCOLOURAMBIENT)
			{
				/* fmad	o[oPrimCol].rpt3,		pa[vPrimCol],			c[cLMSceneAmbi],	c[cMatEmis]; */
				SET_REPEAT_COUNT(uCopySize);
				SETSRCOFF(2, FFTNL_OFFSETS_MATERIAL_EMISSION);
				INST3(FMAD, psOutputColour, psInputColour, psLightModel, psMaterial, "Scene colour - Use vertex colour for material ambient");
			}

			/* No more use for light model constant */
			ReleaseReg(psFFGenCode, psLightModel);
		}
		else
		{
			/* Use normal scene colour */
			psLightModelProduct = GetReg(psFFGenCode,
										 USEASM_REGTYPE_SECATTR,
										 FFGEN_STATE_FRONTLIGHTMODELPRODUCT + uBackFaceIndex,
										 0,
										 4,
										 "Light Model Product");

			/* 
			   If there are no lights then the vertex colour is simply the light model product 

			   If the first enabled light is a spotlight then it's possible that the vertex is outside of the 
			   spotlights cone in which case the output colour will not get initialised. So we need to set
			   it up and not accumulate from the light model product.
			*/
			
			if (!uLightCount || bFirstLightIsSpot)
			{
				SET_REPEAT_COUNT(uCopySize);
				INST1(MOV, psOutputColour, psLightModelProduct, "Scene colour - Use light model product");
				
				/* No more use for light model prduct constant after first light */
				ReleaseReg(psFFGenCode, psLightModelProduct);
				psLightModelProduct = IMG_NULL;
			}
			else
			{
				/* Setup the initial colour accumulate from to be the light product */
				psCurrentColour = psLightModelProduct;
			}
		}

		if (ui32FFTNLEnables1 & FFTNL_ENABLES1_VERTCOLOURDIFFUSE)
		{
			/* If diffuse colour material is enabled then the alpha comes from the vertex colour */
			SETDESTOFF(3); SETSRCOFF(0, 3);
			INST1(MOV, psOutputColour, psInputColour, "Use vertex colour alpha");
		}
		else
		{
			/* Copy the alpha from the diffuse material */
			SETDESTOFF(3);
			SETSRCOFF(0, FFTNL_OFFSETS_MATERIAL_DIFFUSE + 3);
			INST1(MOV, psOutputColour, psMaterial, "Use diffuse material alpha");
		}

		/* Are we doing two sided lighting? */
		if (uBackFaceIndex == 1 && (ui32FFTNLEnables1 & FFTNL_ENABLES1_TRANSFORMNORMALS))
		{
			/* Invert the normal for this pass */
			SET_REPEAT_COUNT(3);
			SETSRCFLAG(0, USEASM_ARGFLAGS_NEGATE);
			INST1(FMOV, psNormal, psNormal, "Invert the normal for calculating the back facing passes");
		}

		uLightCount = 0;
		
		uEnabledLights = (psFFTNLGenDesc->uEnabledSpotLocalLights     |
						  psFFTNLGenDesc->uEnabledSpotInfiniteLights  |
						  psFFTNLGenDesc->uEnabledPointLocalLights    |
						  psFFTNLGenDesc->uEnabledPointInfiniteLights);

		/* loop through all lights */
		while (uEnabledLights)
		{
			IMG_BOOL bSpotLocalLight     = IMG_FALSE;
			IMG_BOOL bSpotInfiniteLight  = IMG_FALSE;
			IMG_BOOL bPointLocalLight    = IMG_FALSE;
			IMG_BOOL bPointInfiniteLight = IMG_FALSE; 

			IMG_CHAR acLightDesc[4][20] = {"Local Spot", "Infinite Spot", "Local Point", "Infinite Point"};

			IMG_CHAR *pszLightDesc = IMG_NULL;

			FFGenReg *psLightSource = IMG_NULL; 

			/* Was specular enabled for this light */
			IMG_BOOL bSpecularPresent = (psFFGenCode->psFFTNLGenDesc->uLightHasSpecular & (1 << uLightCount));

			if (psFFTNLGenDesc->uEnabledSpotLocalLights & (0x1 << uLightCount))
			{
				bSpotLocalLight     = IMG_TRUE;
				pszLightDesc        = acLightDesc[0];
			}
			else if (psFFTNLGenDesc->uEnabledSpotInfiniteLights  & (0x1 << uLightCount))
			{
				bSpotInfiniteLight  = IMG_TRUE;
				pszLightDesc        = acLightDesc[1];
			}
			else if (psFFTNLGenDesc->uEnabledPointLocalLights    & (0x1 << uLightCount))
			{
				bPointLocalLight    = IMG_TRUE;
				pszLightDesc        = acLightDesc[2];
			}
			else if (psFFTNLGenDesc->uEnabledPointInfiniteLights & (0x1 << uLightCount))
			{
				bPointInfiniteLight = IMG_TRUE;
				pszLightDesc        = acLightDesc[3];
			}

			/* Is this light enabled */
			if (uEnabledLights & 0x1)
			{
				NEW_BLOCK(psFFGenCode, "%s Light %d (%s)", pszLightDesc, uLightCount, bSpecularPresent ? "with Specular" : "without Specular");

				COMMENT(psFFGenCode, "Temp size = %d", psFFGenCode->uCurrentTempSize);

				/* Get all the constants for this light.
				   Lights are bound individually so the load offset is zero */
				psLightSource = GetReg(psFFGenCode,
									   USEASM_REGTYPE_SECATTR,
									   FFGEN_STATE_LIGHTSOURCE0 + uLightCount,
									   0,
									   FFTNL_SIZE_LIGHTSOURCE,
									   "Light");

				/* Load the light product */
				psLightProduct = GetReg(psFFGenCode,
										USEASM_REGTYPE_SECATTR,
										FFGEN_STATE_FRONTLIGHTPRODUCT + uBackFaceIndex,
										uLightCount * FFTNL_SIZE_LIGHTPRODUCT,
										FFTNL_SIZE_LIGHTPRODUCT,
										"Light Product");

				if (bPointInfiniteLight)
				{
					FFTNLGenPointInfiniteLighting(psFFGenCode,
												  ui32FFTNLEnables1,
												  psOutputSecColour, 
												  psInputColour, 
												  psOutputColour,
												  psCurrentColour,
												  psMaterial,
												  psLabel,
												  psLightSource,
												  psLightProduct,
												  bSpecularPresent,
												  IMG_FALSE);
				}
				else if (bPointLocalLight)
				{
					FFTNLGenPointLocalLighting(psFFGenCode,
											   ui32FFTNLEnables1,
											   psOutputSecColour,
											   psInputColour,
											   psOutputColour,
											   psCurrentColour,
											   psMaterial,
											   psLabel,
											   psLightSource,
											   psLightProduct,
											   bSpecularPresent);
				}
				else if (bSpotInfiniteLight)
				{
					FFTNLGenSpotInfiniteLighting(psFFGenCode,
												 ui32FFTNLEnables1,
												 psOutputSecColour,
												 psInputColour,
												 psOutputColour,
												 psCurrentColour,
												 psMaterial,
												 psLabel,
												 psLightSource,
												 psLightProduct,
												 bSpecularPresent);
				}
				else if (bSpotLocalLight)
				{
					FFTNLGenSpotLocalLighting(psFFGenCode,
											  ui32FFTNLEnables1,
											  psOutputSecColour,
											  psInputColour,
											  psOutputColour,
											  psCurrentColour,
											  psMaterial,
											  psLabel,
											  psLightSource, 
											  psLightProduct,
											  bSpecularPresent);
				}

				/* Release the light constant */
				ReleaseReg(psFFGenCode, psLightSource);
				psLightSource = IMG_NULL;

				/* Release the light constant */
				ReleaseReg(psFFGenCode, psLightProduct);
				psLightProduct = IMG_NULL;

				COMMENT(psFFGenCode, "Temp size = %d", psFFGenCode->uCurrentTempSize);

				if (psLightModelProduct)
				{
					/* No more use for light model prduct constant after first light */
					ReleaseReg(psFFGenCode, psLightModelProduct);
					
					psCurrentColour = psOutputColour;

					psLightModelProduct = IMG_NULL;
				}
			}	

			/* Shift mask */
			uEnabledLights >>= 1;
			
			/* Increase number of lights */
			uLightCount++;
		}

		/* Are we doing two sided lighting? */
		if (uBackFaceIndex == 1 && (ui32FFTNLEnables1 & FFTNL_ENABLES1_TRANSFORMNORMALS))
		{
			/* Restore the normal */
			SET_REPEAT_COUNT(3);
			SETSRCFLAG(0, USEASM_ARGFLAGS_NEGATE);
			INST1(FMOV, psNormal, psNormal, "Restore the normal");
		}

		ReleaseReg(psFFGenCode, psMaterial);

		/* Now Light the other side if two sided lighting enabled */
		if (bTwoSidedLighting && !uBackFaceIndex)
		{
			uBackFaceIndex = 1;
			goto StartOfLighting;
		}

#if defined(FIX_HW_BRN_25211)
		if(psFFTNLGenDesc->ui32FFTNLEnables2 & FFTNL_ENABLES2_CLAMP_OUTPUT_COLOURS)
		{
			FFGenReg *psOutputColour = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTCOLOR, 0, 4, IMG_NULL);
			FFGenReg *psImmediateFloatReg  = &(psFFGenCode->sImmediateFloatReg);
			FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

			/* Clamp to 0 */
			SET_FLOAT_ZERO(psImmediateFloatReg);

			SETDESTOFF(0); SETSRCOFF(0, 0);
			INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 0 to 0.0");

			SETDESTOFF(1); SETSRCOFF(0, 1);
			INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 1 to 0.0");

			SETDESTOFF(2); SETSRCOFF(0, 2);
			INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 2 to 0.0");

			SETDESTOFF(3); SETSRCOFF(0, 3);
			INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 3 to 0.0");

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

			if(bTwoSidedLighting)
			{
				FFGenReg *psOutputColour = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_BACKCOLOR, 0, 4, IMG_NULL);

				/* Clamp to 0 */
				SET_FLOAT_ZERO(psImmediateFloatReg);

				SETDESTOFF(0); SETSRCOFF(0, 0);
				INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 0 to 0.0");

				SETDESTOFF(1); SETSRCOFF(0, 1);
				INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 1 to 0.0");

				SETDESTOFF(2); SETSRCOFF(0, 2);
				INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 2 to 0.0");

				SETDESTOFF(3); SETSRCOFF(0, 3);
				INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 3 to 0.0");

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

			if (ui32FFTNLEnables1 & FFTNL_ENABLES1_SECONDARYCOLOUR)
			{
				FFGenReg *psOutputColour = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_FRONTSECONDARYCOLOR, 0, 4, IMG_NULL);

				/* Clamp to 0 */
				SET_FLOAT_ZERO(psImmediateFloatReg);

				SETDESTOFF(0); SETSRCOFF(0, 0);
				INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 0 to 0.0");

				SETDESTOFF(1); SETSRCOFF(0, 1);
				INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 1 to 0.0");

				SETDESTOFF(2); SETSRCOFF(0, 2);
				INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 2 to 0.0");

				SETDESTOFF(3); SETSRCOFF(0, 3);
				INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 3 to 0.0");

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


				if(bTwoSidedLighting)
				{
					FFGenReg *psOutputColour = GetReg(psFFGenCode, USEASM_REGTYPE_OUTPUT, FFGEN_OUTPUT_BACKSECONDARYCOLOR, 0, 4, IMG_NULL);

					/* Clamp to 0 */
					SET_FLOAT_ZERO(psImmediateFloatReg);

					SETDESTOFF(0); SETSRCOFF(0, 0);
					INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 0 to 0.0");

					SETDESTOFF(1); SETSRCOFF(0, 1);
					INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 1 to 0.0");

					SETDESTOFF(2); SETSRCOFF(0, 2);
					INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 2 to 0.0");

					SETDESTOFF(3); SETSRCOFF(0, 3);
					INST2(FMAX, psOutputColour, psOutputColour, psImmediateFloatReg, "Clamp colour channel 3 to 0.0");

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
		}	
#endif /* defined(FIX_HW_BRN_25211) */
	}
}	

/******************************************************************************
 End of file (lighting.c)
******************************************************************************/

