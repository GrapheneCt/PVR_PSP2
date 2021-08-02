/*****************************************************************************
 Name           : cdg.h
 
 Title          : Control Dependency Graph interface.
 
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

 Version        : $Revision: 1.3 $

 Modifications  :

 $Log: cdg.h $ 
 *****************************************************************************/
#ifndef __USC_CDG_H
#define __USC_CDG_H

#include "uscshrd.h"

#if defined(EXECPRED)

typedef enum _CTRL_DEP_TYPE
{
	CTRL_DEP_TYPE_BLOCK = 0,
	CTRL_DEP_TYPE_REGION = 1,
} CTRL_DEP_TYPE, *PCTRL_DEP_TYPE;

typedef struct _CTRL_DEP_NODE
{
	CTRL_DEP_TYPE eCtrlDepType;
	union
	{
		struct
		{
			PCODEBLOCK psBlock;
			IMG_UINT32 uNumSucc;
			IMG_UINT32 auSuccIdx[2];
			struct _CTRL_DEP_NODE* apsSucc[2];
			USC_LIST sPredLst;
		} sBlock;
		struct
		{
			USC_LIST sSuccLst;
		} sRegion;		
	} u;
} CTRL_DEP_NODE, *PCTRL_DEP_NODE;
typedef struct _CTRL_DEP_NODE_LISTENTRY
{
	PCTRL_DEP_NODE	psCtrlDepNode;	
	USC_LIST_ENTRY	sListEntry;
} CTRL_DEP_NODE_LISTENTRY, *PCTRL_DEP_NODE_LISTENTRY;

typedef struct _CTRL_DEP_GRAPH
{
	PCTRL_DEP_NODE	psRootCtrlDepNode;
	USC_LIST sCtrlDepBlockLst;
	USC_LIST sCtrlDepRegonLst;
} CTRL_DEP_GRAPH, *PCTRL_DEP_GRAPH;
PCTRL_DEP_GRAPH CalcCtrlDep(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock);
IMG_VOID FreeCtrlDepGraph(PINTERMEDIATE_STATE psState, CTRL_DEP_GRAPH **psCtrlDepGraph);

#endif /* #if defined(EXECPRED) */
#endif /* #ifndef __USC_CDG_H */

/******************************************************************************
 End of file (cdg.h)
******************************************************************************/
