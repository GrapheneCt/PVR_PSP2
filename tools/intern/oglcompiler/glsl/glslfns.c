/******************************************************************************
 * Name         : glslfns.c
 * Author       : James McCarthy
 * Created      : 16/05/2006
 *
 * Copyright    : 2006-2007 by Imagination Technologies Limited.
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
 * $Log: glslfns.c $
 *****************************************************************************/
#include <math.h>

#include "error.h"
#include "glsltree.h"
#include "glslfns.h"

#define GLSLBF0_LOOP for (i = 0;                      i < uLoopCount; i++)
#define GLSLBF1_LOOP for (i = 0, x = 0;               i < uLoopCount; i++, x+= uIncArg[0])
#define GLSLBF2_LOOP for (i = 0, x = 0, y = 0;        i < uLoopCount; i++, x+= uIncArg[0], y+= uIncArg[1])
#define GLSLBF3_LOOP for (i = 0, x = 0, y = 0, z = 0; i < uLoopCount; i++, x+= uIncArg[0], y+= uIncArg[1], z+= uIncArg[2])

#define ANYVEC_TO_BOOLVEC(a) ((GLSLTypeSpecifier)(GLSLTS_BOOL + (GLSLTypeSpecifierNumElementsTable(a) - 1)))






#define PI 3.141592653589793f

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
IMG_INTERNAL IMG_BOOL EmulateBuiltInFunction(GLSLCompilerPrivateData *psCPD,
											 GLSLBuiltInFunctionID  eBFID,
											GLSLTypeSpecifier     *peTypeSpecifiers,
											const IMG_VOID        **pvData, 
											IMG_VOID              *pvResult,
											GLSLTypeSpecifier     *peResultTypeSpecifier)
{
	IMG_UINT32 x, y, z, i;
	IMG_UINT32 uIncArg[3];
	IMG_FLOAT fResult = 0.0f;
	IMG_UINT32 uLoopCount;

	/* Cast some pointers to the possible source types of the parameters */
	const IMG_FLOAT *pfData1  = (IMG_FLOAT *)pvData[0];
	const IMG_FLOAT *pfData2  = (IMG_FLOAT *)pvData[1];
	const IMG_FLOAT *pfData3  = (IMG_FLOAT *)pvData[2];
	IMG_FLOAT *pfResult = (IMG_FLOAT *)pvResult;

	const IMG_BOOL  *pbData1  = (IMG_BOOL *)pvData[0];
	IMG_BOOL  *pbResult = (IMG_BOOL *)pvResult;
	
	const IMG_INT32 *piData1  = (IMG_INT32 *)pvData[0];
	const IMG_INT32 *piData2  = (IMG_INT32 *)pvData[1];

	/* Determine the argument increments */
	uIncArg[0] = (GLSLTypeSpecifierNumElementsTable(peTypeSpecifiers[0]) == 1) ? 0 : 1;
	uIncArg[1] = (GLSLTypeSpecifierNumElementsTable(peTypeSpecifiers[1]) == 1) ? 0 : 1;
	uIncArg[2] = (GLSLTypeSpecifierNumElementsTable(peTypeSpecifiers[2]) == 1) ? 0 : 1;

	/* Get a default loop count */
	uLoopCount = GLSLTypeSpecifierNumElementsTable(peTypeSpecifiers[0]);

	/* Set default result type specifier */
	*peResultTypeSpecifier = peTypeSpecifiers[0];

	switch (eBFID)
	{
		case GLSLBFID_RADIANS:
			GLSLBF1_LOOP { pfResult[i] = PI/180.0f * pfData1[x]; }
			break;
		case GLSLBFID_DEGREES:
			GLSLBF1_LOOP { pfResult[i] = 180.0f/PI * pfData1[x]; }
			break;
		case GLSLBFID_SIN:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)sin(pfData1[x]); }
			break;
		case GLSLBFID_COS:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)cos(pfData1[x]); }
			break;
		case GLSLBFID_TAN:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)tan(pfData1[x]); }
			break;
		case GLSLBFID_ASIN:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)asin(pfData1[x]); }
			break;
		case GLSLBFID_ACOS:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)acos(pfData1[x]); }
			break;
		case GLSLBFID_ATAN:
			GLSLBF2_LOOP
			{
				if (pfData2)
				{
					GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)atan2(pfData2[y], pfData1[x]); }
				}
				else
				{
					GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)atan(pfData1[x]); }
				}
			}
			break;
		case GLSLBFID_POW:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)pow(pfData1[x], pfData2[x]); }
			break;
		case GLSLBFID_EXP:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)exp(pfData1[x]); }
			break;
		case GLSLBFID_LOG:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)log(pfData1[x]); }
			break;
		case GLSLBFID_EXP2:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)pow(2.0f, pfData1[x]); }
			break;
		case GLSLBFID_LOG2:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)(log(pfData1[x]) / log(2.0f)); }
			break;
		case GLSLBFID_SQRT:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)sqrt(pfData1[x]); }
			break;
		case GLSLBFID_INVERSESQRT:
			GLSLBF1_LOOP { pfResult[i] = 1.0f / (IMG_FLOAT)sqrt(pfData1[x]); }
			break;
		case GLSLBFID_ABS:
			GLSLBF1_LOOP 
			{
				{
					pfResult[i] = (IMG_FLOAT)fabs(pfData1[x]);
				}							
			}
			break;
		case GLSLBFID_SIGN:
			GLSLBF1_LOOP
			{
				{
					pfResult[i] = (pfData1[x] > 0.0f) ? 1.0f : ((pfData1[x] < 0.0f) ? -1.0f : 0.0f);
				}
			}
			break;
		case GLSLBFID_FLOOR:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)floor(pfData1[x]); }
			break;
		case GLSLBFID_CEIL:
			GLSLBF1_LOOP { pfResult[i] = (IMG_FLOAT)ceil(pfData1[x]); }
			break;
		case GLSLBFID_FRACT:
			GLSLBF1_LOOP { pfResult[i] = pfData1[x] - (IMG_FLOAT)floor(pfData1[x]); }
			break;
		case GLSLBFID_MOD:
			GLSLBF2_LOOP { pfResult[i] = pfData1[x] - (pfData2[y] * (IMG_FLOAT)floor(pfData1[x] / pfData2[y])); }
			break;
		case GLSLBFID_MIN:
			GLSLBF2_LOOP
			{
				{
					GLSLBF2_LOOP { pfResult[i] = pfData2[y] < pfData1[x] ? pfData2[y] : pfData1[x]; }
				}
			}
			break;
		case GLSLBFID_MAX:
			GLSLBF2_LOOP
			{
				{
					GLSLBF2_LOOP { pfResult[i] = pfData1[x] < pfData2[y] ? pfData2[y] : pfData1[x]; }
				}
			}
			break;
		case GLSLBFID_CLAMP:
			GLSLBF3_LOOP
			{
				{
					if (pfData1[x] < pfData2[y])
					{
						pfResult[i] = pfData2[y];
					}
					else if (pfData1[x] > pfData3[z])
					{ 
						pfResult[i] = pfData3[z];
					}
					else
					{
						pfResult[i] = pfData1[x];
					}
				}
			}
			break;
		case GLSLBFID_MIX:
			GLSLBF3_LOOP { pfResult[i] = (pfData1[x] * (1 - pfData3[z])) + (pfData2[y] * pfData3[z]); }
			break;
		case GLSLBFID_STEP:
			GLSLBF2_LOOP { pfResult[i] = pfData1[x] < pfData2[y] ? 0.0f : 1.0f; }
			*peResultTypeSpecifier = peTypeSpecifiers[1];
			break;
		case GLSLBFID_SMOOTHSTEP:
			*peResultTypeSpecifier = peTypeSpecifiers[2];
			break;
		case GLSLBFID_LENGTH:
			GLSLBF1_LOOP { fResult += (pfData1[x] * pfData1[x]); }
			pfResult[0] = (IMG_FLOAT)sqrt(fResult);
			*peResultTypeSpecifier = GLSLTS_FLOAT;
			break;
		case GLSLBFID_DISTANCE:
			GLSLBF2_LOOP { pfResult[i] = pfData1[x] - pfData2[y]; }
			GLSLBF1_LOOP { fResult += (pfResult[x] * pfResult[x]); }
			pfResult[0] = (IMG_FLOAT)sqrt(fResult);
			*peResultTypeSpecifier = GLSLTS_FLOAT;
			break;
		case GLSLBFID_DOT:
			GLSLBF2_LOOP { fResult += (pfData1[x] * pfData2[y]); }
			pfResult[0] = fResult;
			*peResultTypeSpecifier = GLSLTS_FLOAT;
			break;
		case GLSLBFID_CROSS:
			pfResult[0] = (pfData1[1] * pfData2[2]) - (pfData2[1] * pfData1[2]);
			pfResult[1] = (pfData1[2] * pfData2[0]) - (pfData2[2] * pfData1[0]);
			pfResult[2] = (pfData1[0] * pfData2[1]) - (pfData2[0] * pfData1[1]);
			break;
		case GLSLBFID_NORMALIZE:
			GLSLBF1_LOOP { fResult += (pfData1[x] * pfData1[x]); }
			fResult = 1.0f / (IMG_FLOAT)sqrt(fResult);
			GLSLBF1_LOOP { pfResult[i] = (pfData1[x] * fResult); }
			break;
		case GLSLBFID_FACEFORWARD:
			GLSLBF3_LOOP { fResult += (pfData2[y] * pfData3[z]); }
			GLSLBF1_LOOP { pfResult[i] = fResult < 0 ? pfData1[x] : -pfData1[x]; }
			break;
		case GLSLBFID_REFLECT:
			/* Calc dot(N,I) */
			GLSLBF2_LOOP { fResult += (pfData1[x] * pfData2[y]); }
			/* Calc I - 2 * dot(N,I) * N */
			GLSLBF2_LOOP { pfResult[i] = pfData1[x] - ((2.0f * fResult) * pfData2[y]); }
			break;
		case GLSLBFID_REFRACT:
			/* Calc dot(N,I) */
			GLSLBF2_LOOP { fResult += (pfData1[x] * pfData2[y]); }
			/* Calc 1.0 - ( dot(N,I) * dot(N,I) ) */
			fResult = 1.0f - (fResult * fResult);
			/* Calc k = 1.0 - eta * eta * (1.0 - dot(N,I) * dot(N,I)) */
			fResult = 1.0f - (pfData3[0] * pfData3[0] * fResult);
			/* If k < 0.0 */
			if (fResult < 0.0f)	

			{
				GLSLBF0_LOOP { pfResult[i] = 0.0f; }
			}
			else
			{
				IMG_FLOAT fDotNI = 0.0f;
				/* Calc dot(N,I) */
				GLSLBF2_LOOP { fDotNI += (pfData1[x] * pfData2[y]); }
				/* Calc (eta * dot(N, I) + sqrt(k)) */
				fResult = pfData3[0] * fDotNI * (IMG_FLOAT)sqrt(fResult);
				/* eta * I - (eta * dot(N, I) + sqrt(k)) * N */
				GLSLBF3_LOOP { pfResult[i] = (pfData3[0] * pfData1[x]) - (fResult * pfData2[y]); }
			}
			break;
		case GLSLBFID_MATRIXCOMPMULT:
			GLSLBF2_LOOP { pfResult[i] = pfData1[x] * pfData2[y]; }
			break;
		case GLSLBFID_TRANSPOSE:
		{
			IMG_UINT32 uNumRows = GLSLTypeSpecifierIndexedTable(peTypeSpecifiers[0]);
			IMG_UINT32 uNumCols = GLSLTypeSpecifierDimensionTable(peTypeSpecifiers[0]);

			static const GLSLTypeSpecifier aeTransposeResultTable[9] = {GLSLTS_MAT2X2, GLSLTS_MAT3X2, GLSLTS_MAT4X2,
																		GLSLTS_MAT2X3, GLSLTS_MAT3X3, GLSLTS_MAT4X3,
																		GLSLTS_MAT2X4, GLSLTS_MAT3X4, GLSLTS_MAT4X4};
			/* Get result type specifier */
			*peResultTypeSpecifier = aeTransposeResultTable[peTypeSpecifiers[0] - GLSLTS_MAT2X2];

			/* Transpose the matrix */
			for (i = 0; i < uNumRows; i++)
			{
				for (x = 0; x < uNumCols; x++)
				{
					pfResult[(i * uNumCols) + x] = pfData1[(x * uNumRows) + i];  
				}
			}

			break;
		}
		case GLSLBFID_OUTERPRODUCT:
		{
			IMG_UINT32 uNumRows = GLSLTypeSpecifierDimensionTable(peTypeSpecifiers[0]);
			IMG_UINT32 uNumCols = GLSLTypeSpecifierDimensionTable(peTypeSpecifiers[1]);

			static const GLSLTypeSpecifier aeOuterProductResultTable[3][3] = {{GLSLTS_MAT2X2, GLSLTS_MAT3X2, GLSLTS_MAT4X2},
																			  {GLSLTS_MAT2X3, GLSLTS_MAT3X3, GLSLTS_MAT4X3},
																			  {GLSLTS_MAT2X4, GLSLTS_MAT3X4, GLSLTS_MAT4X4}};
			/* Get result type specifier */
			*peResultTypeSpecifier = aeOuterProductResultTable[peTypeSpecifiers[0] - GLSLTS_VEC2][peTypeSpecifiers[1] - GLSLTS_VEC2];

			/* Do the outer product */
			for (i = 0; i < uNumRows; i++)
			{
				for (x = 0; x < uNumCols; x++)
				{
					pfResult[(i * uNumCols) + x] = pfData1[i] * pfData2[x]; 
				}
			}

			break;
		}
		case GLSLBFID_LESSTHAN:
			if (GLSL_IS_FLOAT(peTypeSpecifiers[0]))
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(pfData1[x] < pfData2[y]); }
			}
			else
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(piData1[x] < piData2[y]); }
			}
			*peResultTypeSpecifier = ANYVEC_TO_BOOLVEC(peTypeSpecifiers[0]);
			break;
		case GLSLBFID_LESSTHANEQUAL:
			if (GLSL_IS_FLOAT(peTypeSpecifiers[0]))
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(pfData1[x] <= pfData2[y]); }
			}
			else
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(piData1[x] <= piData2[y]); }
			}
			*peResultTypeSpecifier = ANYVEC_TO_BOOLVEC(peTypeSpecifiers[0]);
			break;
		case GLSLBFID_GREATERTHAN:
			if (GLSL_IS_FLOAT(peTypeSpecifiers[0]))
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(pfData1[x] > pfData2[y]); }
			}
			else
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(piData1[x] > piData2[y]); }
			}
			*peResultTypeSpecifier = ANYVEC_TO_BOOLVEC(peTypeSpecifiers[0]);
			break;
		case GLSLBFID_GREATERTHANEQUAL:
			if (GLSL_IS_FLOAT(peTypeSpecifiers[0]))
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(pfData1[x] >= pfData2[y]); }
			}
			else
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(piData1[x] >= piData2[y]); }
			}
			*peResultTypeSpecifier = ANYVEC_TO_BOOLVEC(peTypeSpecifiers[0]);
			break;
		case GLSLBFID_EQUAL:
			if (GLSL_IS_FLOAT(peTypeSpecifiers[0]))
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(pfData1[x] == pfData2[y]); }
			}
			else
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(piData1[x] == piData2[y]); }
			}
			*peResultTypeSpecifier = ANYVEC_TO_BOOLVEC(peTypeSpecifiers[0]);
			break;
		case GLSLBFID_NOTEQUAL:
			if (GLSL_IS_FLOAT(peTypeSpecifiers[0]))
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(pfData1[x] != pfData2[y]); }
			}
			else
			{
				GLSLBF2_LOOP { pbResult[i] = (IMG_BOOL)(piData1[x] != piData2[y]); }
			}
			*peResultTypeSpecifier = ANYVEC_TO_BOOLVEC(peTypeSpecifiers[0]);
			break;
		case GLSLBFID_ANY:
			pbResult[0] = IMG_FALSE;
			GLSLBF1_LOOP 
			{ 
				if (pbData1[x]) 
				{
					pbResult[0] = IMG_TRUE; 
				}
			}
			*peResultTypeSpecifier = GLSLTS_BOOL;
			break;
		case GLSLBFID_ALL:
			pbResult[0] = IMG_TRUE;
			GLSLBF1_LOOP 
			{ 
				if (!pbData1[x]) 
				{
					pbResult[0] = IMG_FALSE; 
				}
			}
			*peResultTypeSpecifier = GLSLTS_BOOL;
			break;
		case GLSLBFID_NOT:
			GLSLBF1_LOOP { pbResult[i] = (IMG_BOOL)(!pbData1[x]); }
			*peResultTypeSpecifier = ANYVEC_TO_BOOLVEC(peTypeSpecifiers[0]);
			break;
		case GLSLBFID_DFDX:
		case GLSLBFID_DFDY:
		case GLSLBFID_FWIDTH:
			GLSLBF0_LOOP { pfResult[i] = 0.0f; }
			break;
		case GLSLBFID_FTRANSFORM:
		case GLSLBFID_TEXTURE:
		case GLSLBFID_TEXTUREPROJ:
		case GLSLBFID_TEXTURELOD:
		case GLSLBFID_TEXTUREPROJLOD:
		case GLSLBFID_TEXTUREGRAD:
		case GLSLBFID_TEXTURESTREAM:
		case GLSLBFID_TEXTURESTREAMPROJ:
		case GLSLBFID_NOISE1:
		case GLSLBFID_NOISE2:
		case GLSLBFID_NOISE3:
		case GLSLBFID_NOISE4:
			return IMG_FALSE;
		default:
		{
			LOG_INTERNAL_ERROR(("EmulateBuiltInFunction: Unrecognised built in function ID (%08X)\n", eBFID));
			return IMG_FALSE;
		}

	}

	return IMG_TRUE;
}
/******************************************************************************
 End of file (glslfns.c)
******************************************************************************/
