/*****************************************************************************
 Name           : LAYOUT.H
 
 Title          : USC Program layout interface.
 
 C Author       : Rana-Iftikhar Ahmad
 
 Created        : 21/11/2008
 
 Copyright      : 2002-2007 by Imagination Technologies Ltd.
                  All rights reserved. No part of this software, either
                  material or conceptual may be copied or distributed,
                  transmitted, transcribed, stored in a retrieval system or
                  translated into any human or computer language in any form
                  by any means, electronic, mechanical, manual or otherwise,
                  or disclosed to third parties without the express written
                  permission of Imagination Technologies Ltd,
                  Home Park Estate, Kings Langley, Hertfordshire,
                  WD4 8LZ, U.K.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.8 $

 Modifications  :

 $Log: layout.h $ 
*****************************************************************************/
#ifndef __USC_LAYOUT_H
#define __USC_LAYOUT_H
struct _LAYOUT_INFO_;
typedef IMG_VOID (*LAY_BRANCH)(PINTERMEDIATE_STATE psState, struct _LAYOUT_INFO_ *psLayout, IOPCODE eOpcode, IMG_PUINT32 puDestLabel, IMG_UINT32 uPredSrc, IMG_BOOL bPredNegate, IMG_BOOL bSyncEnd, EXECPRED_BRANCHTYPE eExecPredBranchType);
typedef IMG_VOID (*LAY_LABEL)(PINTERMEDIATE_STATE psState, struct _LAYOUT_INFO_ *psLayout, IMG_UINT32 uDestLabel, IMG_BOOL bSyncEndDest);
typedef IMG_PUINT32 (*LAY_INSTRS)(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PUINT32 puInst, struct _LAYOUT_INFO_* psLayout);
typedef enum _LAYOUT_POINT_
{
	LAYOUT_INIT, 
	LAYOUT_MAIN_PROG_START, 
	LAYOUT_SUB_ROUT_END, 
	LAYOUT_MAIN_PROG_END,  
	LAYOUT_SA_PROG_START, 
	LAYOUT_SA_PROG_END, 
	LAYOUT_POST_FEEDBACK_START, 
	LAYOUT_POST_FEEDBACK_END, 
	LAYOUT_POST_SPLIT_START, 
	LAYOUT_LAST_POINT
} LAYOUT_POINT;
typedef IMG_BOOL (*LAY_POINT_ACTIONS)(PINTERMEDIATE_STATE psState, struct _LAYOUT_INFO_ *psLayout, LAYOUT_POINT eLayoutPoint);
typedef IMG_VOID (*LAY_ALIGN_TO_EVEN)(PINTERMEDIATE_STATE psState, struct _LAYOUT_INFO_ *psLayout);

typedef struct _LAYOUT_INFO_ {
	IMG_PUINT32 puInst;
	IOPCODE eLastOpcode;
	IMG_BOOL bLastLabelWasSyncEndDest;
	IMG_BOOL bThisLabelIsSyncEndDest;
	LAY_BRANCH pfnBranch;
	LAY_LABEL pfnLabel;
	LAY_POINT_ACTIONS pfnPointActions;
	LAY_ALIGN_TO_EVEN pfnAlignToEven;
	LAY_INSTRS pfnInstrs;
	/* label for start of each block in current function */
	IMG_PUINT32 auLabels;
	IMG_PUINT32 puMainProgInstrs;
} LAYOUT_INFO, *PLAYOUT_INFO;

IMG_VOID NoSchedLabelCB(PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout, IMG_UINT32 uDestLabel, IMG_BOOL bSyncEndDest);
IMG_VOID CommonBranchCB(PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout, IOPCODE eOpcode, IMG_PUINT32 puDestLabel, IMG_UINT32 uPredSrc, IMG_BOOL bPredNegate, IMG_BOOL bSyncEndDest);

typedef enum _LAYOUT_PROGRAM_PARTS_
{
	LAYOUT_MAIN_PROGRAM, 
	LAYOUT_SA_PROGRAM, 
	LAYOUT_WHOLE_PROGRAM
} LAYOUT_PROGRAM_PARTS;
IMG_VOID LayoutProgram(PINTERMEDIATE_STATE psState, LAY_INSTRS pfnInstrs, LAY_BRANCH pfnBranch, LAY_LABEL pfnLabel, LAY_POINT_ACTIONS pfnPointActions, LAY_ALIGN_TO_EVEN pfnAlignToEven, LAYOUT_PROGRAM_PARTS eLayoutProgramParts);
IMG_BOOL PointActionsHwCB(PINTERMEDIATE_STATE psState, LAYOUT_INFO *psLayout, LAYOUT_POINT eLayoutPoint);
IMG_VOID AppendInstruction(PLAYOUT_INFO psLayout, IOPCODE eOpcode);
#endif
/******************************************************************************
 End of file (layout.h)
******************************************************************************/
