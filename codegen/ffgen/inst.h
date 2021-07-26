/**************************************************************************
 * Name         : inst.h
 * Author       : James McCarthy
 * Created      : 16/11/2005
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
 * $Log: inst.h $
 **************************************************************************/

#ifndef __gl_inst_h_
#define __gl_inst_h_

#include "codegen.h"

#ifdef FFGEN_UNIFLEX
#define IF_FFGENCODE_UNIFLEX (psFFGenCode->eProgramType == FFGENPT_TNL && /* We use UniFlex instructions to create only TNL programs, */  \
	!(psFFGenCode->eCodeGenFlags & FFGENCGF_REDIRECT_OUTPUT_TO_INPUT))  /* and only TNL programs that are not associated with geometry shaders */

IMG_INTERNAL IMG_VOID AddUniFlexInstructionToList(FFGenCode *psFFGenCode, UNIFLEX_INST *psInstToAdd);
#endif

IMG_INTERNAL IMG_VOID IssueWDFs(FFGenCode     *psFFGenCode,
				   FFGenInstruction *psInstruction);

IMG_INTERNAL IMG_VOID EncodeInstructionfn(FFGenCode     *psFFGenCode,
							 FFGenInstruction *psInstruction,
							 IMG_UINT32        uLineNumber);

#define EncodeInstruction(a, b) EncodeInstructionfn(a, b, __LINE__)

IMG_INTERNAL IMG_VOID EncodeInstructionList(FFGenCode     *psFFGenCode);
IMG_INTERNAL IMG_VOID DestroyInstructionList(FFGenContext *psFFGenContext, UseInstructionList *psList);

IMG_INTERNAL IMG_UINT32 GetDRC(FFGenCode *psFFGenCode);

IMG_INTERNAL IMG_UINT32 GetLabel(FFGenCode *psFFGenCode, IMG_CHAR *pszLabelName);
IMG_INTERNAL IMG_VOID DestroyLabelList(FFGenCode *psFFGenCode);


IMG_INTERNAL IMG_VOID M4x4(FFGenCode *psFFGenCode, FFGenReg *psDest, FFGenReg *psVector, FFGenReg *psMatrix, IMG_UINT32 uDestBaseOffset, IMG_CHAR *pszComment);
IMG_INTERNAL IMG_VOID M4x3(FFGenCode *psFFGenCode, FFGenReg *psDest, FFGenReg *psVector, FFGenReg *psMatrix, IMG_CHAR *pszComment);

IMG_INTERNAL IMG_VOID IF_PRED(FFGenCode *psFFGenCode, FFGenReg *psPredReg, IMG_CHAR *pszDesc, IMG_CHAR *pszComment);
IMG_INTERNAL IMG_VOID ELSE_PRED(FFGenCode *psFFGenCode, IMG_CHAR *pszDesc, IMG_CHAR *pszComment);
IMG_INTERNAL IMG_VOID END_PRED(FFGenCode *psFFGenCode, IMG_CHAR *pszComment);


IMG_INTERNAL IMG_VOID IMG_CALLCONV COMMENT(FFGenCode *psFFGenCode, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);
IMG_INTERNAL IMG_VOID IMG_CALLCONV NEW_BLOCK(FFGenCode *psFFGenCode, const IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);

IMG_INTERNAL IMG_VOID IssueOutstandingWDFs(FFGenCode *psFFGenCode);


#endif // __gl_inst_h_
