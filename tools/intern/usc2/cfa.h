/*****************************************************************************
 Name           : cdg.h
 
 Title          : Control Flow Analysis interface.
 
 C Author       : Rana-Iftikhar Ahmad
 
 Created        : 27/04/2011
 
 Copyright      : 2002-2011 by Imagination Technologies Ltd.
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

 Version        : $Revision: 1.4 $

 Modifications  :

 $Log: cfa.h $ 
 *****************************************************************************/
#ifndef __USC_CFA_H
#define __USC_CFA_H

#include "uscshrd.h"

#if defined(EXECPRED)
typedef struct _LOOP_INFO
{
	PCODEBLOCK			psLoopHeaderBlock;
	USC_LIST			sLoopOutFlowLst;
	USC_LIST			sLoopCntEdgeLst;
	struct _LOOP_INFO *	psEnclosingLoopInfo;
} LOOP_INFO, *PLOOP_INFO;

typedef struct _LOOP_INFO_LISTENTRY
{
	LOOP_INFO		sLoopInfo;
	USC_LIST_ENTRY	sListEntry;
} LOOP_INFO_LISTENTRY, *PLOOP_INFO_LISTENTRY;

typedef struct _FUNC_LOOPS_INFO
{
	PUSC_FUNCTION	psFunc;
	USC_LIST		sFuncLoopInfoLst;
} FUNC_LOOPS_INFO, *PFUNC_LOOPS_INFO;

typedef struct _FUNC_LOOP_INFO_LISTENTRY
{
	FUNC_LOOPS_INFO	sFuncLoopsInfo;
	USC_LIST_ENTRY	sListEntry;
} FUNC_LOOPS_INFO_LISTENTRY, *PFUNC_LOOPS_INFO_LISTENTRY;
IMG_VOID RestructureLoop(PINTERMEDIATE_STATE psState, PLOOP_INFO psLoopInfo,  PCFG *ppsLoopCfg, IMG_PBOOL pbGenerateInfiniteLoop, IMG_PUINT32 puLoopPredReg, IMG_PBOOL pbInvertLoopPredReg, PUSC_LIST psLoopInfoLst);
IMG_VOID FindProgramLoops(PINTERMEDIATE_STATE psState, PUSC_LIST psFuncLoopsInfoLst);
IMG_VOID FreeFuncLoopsInfoList(PINTERMEDIATE_STATE psState, PUSC_LIST psFuncLoopsInfoLst);

#endif /* #if defined(EXECPRED) */
#endif /* #ifndef __USC_CFA_H */

/******************************************************************************
 End of file (cfa.h)
******************************************************************************/
