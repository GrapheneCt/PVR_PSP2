/**************************************************************************
 * Name         : reg.h
 * Author       : James McCarthy
 * Created      : 16/11/2005
 *
 * Copyright    : 2000-2008 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: reg.h $
 **************************************************************************/

#ifndef __gl_reg_h_
#define __gl_reg_h_

#include "codegen.h"

/*	
	GetReg(psFFGenCode, eRegType, eBindingRegDesc, uLoadOffsetInDWords, uSize, pszDesc, );
	GetIndexedReg(psFFGenCode, eRegType, eBindingRegDesc, psIndexReg, uSize, pszDesc)
	AllocRegSpace(psFFGenCode, eRegType, eBindingRegDesc, uSize, bIndexableSecondary)
  	LoadReg(psFFGenCode, eBindingRegDesc, uLoadOffsetInDWords, uSize, pszDesc, bFCLLineLoad);
*/

#define GetReg(a,b,c,d,e,f)       GetRegfn(a,b,c,d,       IMG_NULL,e,f,       IMG_FALSE,IMG_FALSE,IMG_FALSE, __LINE__,__FILE__)
#define GetIndexedReg(a,b,c,d,e,f)GetRegfn(a,b,c,0,       d,       e,f,       IMG_FALSE,IMG_FALSE,IMG_FALSE, __LINE__,__FILE__)
#define AllocRegSpace(a,b,c,d,e)  GetRegfn(a,b,c,0,       IMG_NULL,d,IMG_NULL,IMG_TRUE, e,        IMG_FALSE, __LINE__,__FILE__)
#define LoadReg(a,c,d,e,f, g)	  GetRegfn(a,USEASM_REGTYPE_SECATTR,c,d,      IMG_NULL,e,f,       IMG_FALSE,IMG_FALSE,g, __LINE__,__FILE__)


/*
	StoreReg(psFFGenCode, eBindingRegDesc,uOffsetInDWords,uSizeInDWords,psStoreSrc, bIsuueWDFImmediately, pszDesc)
	StoreIndexedReg(psFFGenCode, eBindingRegDesc,psIndexReg, uSizeInDWords,psStoreSrc, bIsuueWDFImmediately, pszDesc)
*/
#define StoreReg(a,c,d,e,f,g,h)			StoreRegfn(a,c,IMG_NULL,d,e,f,g,h, __LINE__, __FILE__)
#define StoreIndexReg(a,c,d,e,f,g,h)	StoreRegfn(a,c,d,       0,e,f,g,h, __LINE__, __FILE__)

typedef struct FFGENIndexRegTAG
{
	FFGenReg     *psReg;
	IMG_UINT32    uStrideInDWords;
	IMG_UINT32    uIndexRegFlags;
	IMG_UINT32    uIndexRegOffset;
} FFGENIndexReg;


FFGenReg *GetRegfn(FFGenCode  *psFFGenCode,
				   UseasmRegType  eType,
				   FFGenRegDesc   eBindingRegDesc,
				   IMG_UINT32     uLoadOffsetInDWords,
				   FFGENIndexReg *psIndexReg,
				   IMG_UINT32     uSize,
				   IMG_CHAR      *pszDesc,
				   IMG_BOOL       bAllocSpaceOnly,
				   IMG_BOOL       bIndexableSecondary,
				   IMG_BOOL		  bFCLFillLoad,
				   IMG_UINT32     uLineNumber,
				   IMG_CHAR      *pszFileName);



IMG_BOOL StoreRegfn(FFGenCode			*psFFGenCode,
					FFGenRegDesc		eBindingRegDesc,
					FFGENIndexReg		*psIndexReg,
					IMG_UINT32			uOffsetInDWords,
					IMG_UINT32			uSizeInDWords,
					FFGenReg			*psStoreSrc,
					IMG_BOOL			bIssueWDFImmediately,
					IMG_CHAR			*pszDesc,
					IMG_UINT32			uLineNumber,
					IMG_CHAR			*pszFileName);

IMG_INTERNAL IMG_VOID ReleaseReg(FFGenCode *psFFGenCode, FFGenReg     *psReg);

IMG_INTERNAL IMG_VOID DestroyRegList(FFGenContext *psFFGenContext, FFGenRegList *psList, IMG_BOOL bFreeReg);


#endif /* __gl_reg_h_ */
