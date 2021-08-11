/**************************************************************************
 * Name         : icemul.h
 * Created      : 11/03/2005
 *
 * Copyright    : 2005 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: icemul.h $
 **************************************************************************/

#ifndef __gl_icemul_h_
#define __gl_icemul_h_

IMG_VOID ICEmulateBIFNtan(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand);
IMG_VOID ICEmulateBIFNpow(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand);
IMG_VOID ICEmulateBIFNfract(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand);
IMG_VOID ICEmulateBIFNmod(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand);

IMG_VOID ICEmulateBIFNshadowFunc(GLSLCompilerPrivateData *psCPD, 
								 GLSLICProgram          *psICProgram, 
								 GLSLNode				*psNode,
								 GLSLICOperandInfo		*psDestOperand,
								 GLSLBuiltInFunctionID	eShadowFuncID);

IMG_VOID ICEmulateBIFNArcTrigs(GLSLCompilerPrivateData *psCPD, 
							   GLSLICProgram		*psICProgram, 
							   GLSLNode				*psNode,
							   GLSLICOperandInfo	*psDestOperand,
							   GLSLBuiltInFunctionID eArcTrigFuncID);

IMG_VOID ICEmulateBIFNnoise1(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand);
IMG_VOID ICEmulateBIFNnoise2(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand);
IMG_VOID ICEmulateBIFNnoise3(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand);
IMG_VOID ICEmulateBIFNnoise4(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand);


/* 
** Add ArcCos, AecSin and ArcTan Function body for 
** calculating arccos, arcsin and arc tan of a single value 
*/
IMG_BOOL ICAddArcSinCos2FunctionBody(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, IMG_BOOL bCos);
IMG_BOOL ICAddArcTanFunctionBody(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram);
IMG_BOOL ICAddArcTan2FunctionBody(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram);

/* 
** Add function body for four different dimensional noise function 
**/
IMG_BOOL ICAddNoise1DFunctionBody(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram);
IMG_BOOL ICAddNoise2DFunctionBody(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram);
IMG_BOOL ICAddNoise3DFunctionBody(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram);
IMG_BOOL ICAddNoise4DFunctionBody(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram);

/* Insert code for fog factor */
IMG_BOOL ICInsertCodeForFogFactor(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram);

/* Emulate texture lookup with projective division */
IMG_VOID ICEmulateTextureProj(GLSLCompilerPrivateData *psCPD, 
							  GLSLICProgram		*psICProgram, 
							  GLSLNode			*psNode, 
							  GLSLICOperandInfo *psDestOperand,
							  GLSLICOpcode		eTextureLookupOp);

IMG_VOID ICEmulateTextureRect(GLSLCompilerPrivateData	*psCPD,
							  GLSLICProgram				*psICProgram,
							  GLSLNode					*psNode,
							  GLSLICOperandInfo			*psDestOperand);


#endif /* __gl_icemul_h_ */

