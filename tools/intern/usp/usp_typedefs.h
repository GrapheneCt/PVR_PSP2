 /*****************************************************************************
 Name			: USP_TYPEDEFS.H
 
 Title			: typedefs for ALL the USP data-types
 
 C Author 		: Joe Molleson
 
 Created  		: 02/01/2002
 
 Copyright      : 2002-2006 by Imagination Technologies Limited. All rights reserved.
                : No part of this software, either material or conceptual 
                : may be copied or distributed, transmitted, transcribed,
                : stored in a retrieval system or translated into any 
                : human or computer language in any form by any means,
                : electronic, mechanical, manual or other-wise, or 
                : disclosed to third parties without the express written
                : permission of Imagination Technologies Limited, Unit 8, HomePark
                : Industrial Estate, King's Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description 	: Typedefs for ALL the USP data-types
   
 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.23 $

 Modifications	:

 $Log: usp_typedefs.h $
*****************************************************************************/
#ifndef _USP_TYPEDEFS_H_
#define _USP_TYPEDEFS_H_

typedef IMG_PVOID (IMG_CALLCONV *USP_MEMALLOCFN)( IMG_UINT32 uSize );
typedef IMG_VOID (IMG_CALLCONV *USP_MEMFREEFN)( IMG_PVOID pvData );
typedef IMG_VOID (IMG_CALLCONV *USP_DBGPRINTFN)( const IMG_CHAR* pszFormat, ... ) IMG_FORMAT_PRINTF(1, 2);

/*
	Typedef's for types defined in hwinst.h
*/
typedef struct	_HW_INST_		HW_INST,		*PHW_INST;
typedef struct	_USP_REG_		USP_REG,		*PUSP_REG;

/*
	Typedef's for types defined in uspbin.h
*/
typedef struct	_USP_PC_BLOCK_HDR_				USP_PC_BLOCK_HDR,			*PUSP_PC_BLOCK_HDR;
typedef struct	_USP_PC_CONST_RANGE_			USP_PC_CONST_RANGE,			*PUSP_PC_CONST_RANGE;
typedef struct	_USP_PC_INDEXABLE_TEMP_SIZE_	USP_PC_INDEXABLE_TEMP_SIZE,	*PUSP_PC_INDEXABLE_TEMP_SIZE;
typedef struct	_USP_PC_PSINPUT_LOAD_			USP_PC_PSINPUT_LOAD,		*PUSP_PC_PSINPUT_LOAD;
typedef struct	_USP_PC_CONST_LOAD_				USP_PC_CONST_LOAD,			*PUSP_PC_CONST_LOAD;
typedef struct	_USP_PC_TEXSTATE_LOAD_			USP_PC_TEXSTATE_LOAD,		*PUSP_PC_TEXSTATE_LOAD;
typedef struct	_USP_PC_BLOCK_PROGDESC_			USP_PC_BLOCK_PROGDESC,		*PUSP_PC_BLOCK_PROGDESC;
typedef struct	_USP_PC_INSTDESC_				USP_PC_INSTDESC,			*PUSP_PC_INSTDESC;
typedef struct	_USP_PC_MOESTATE_				USP_PC_MOESTATE,			*PUSP_PC_MOESTATE;
typedef struct	_USP_PC_BLOCK_HWCODE_			USP_PC_BLOCK_HWCODE,		*PUSP_PC_BLOCK_HWCODE;
typedef struct	_USP_PC_BLOCK_SAMPLE_			USP_PC_BLOCK_SAMPLE,		*PUSP_PC_BLOCK_SAMPLE;
typedef struct	_USP_PC_BLOCK_SAMPLEUNPACK_		USP_PC_BLOCK_SAMPLEUNPACK,	*PUSP_PC_BLOCK_SAMPLEUNPACK;
typedef struct	_USP_PC_DR_SAMPLE_DATA_			USP_PC_DR_SAMPLE_DATA,		*PUSP_PC_DR_SAMPLE_DATA;
typedef struct	_USP_PC_NONDR_SAMPLE_DATA_		USP_PC_NONDR_SAMPLE_DATA,	*PUSP_PC_NONDR_SAMPLE_DATA;
typedef struct	_USP_PC_BLOCK_BRANCH_			USP_PC_BLOCK_BRANCH,		*PUSP_PC_BLOCK_BRANCH;
typedef struct	_USP_PC_BLOCK_LABEL_			USP_PC_BLOCK_LABEL,			*PUSP_PC_BLOCK_LABEL;
typedef struct  _USP_PC_BLOCK_TEXTURE_WRITE_      USP_PC_BLOCK_TEXTURE_WRITE,   *PUSP_PC_BLOCK_TEXTURE_WRITE;

/*
	Typedef's for all the types defined in uspshrd.h 
*/
typedef struct	_USP_SAMPLER_DESC_			USP_SAMPLER_DESC,		*PUSP_SAMPLER_DESC;
typedef struct	_USP_CONTEXT_				USP_CONTEXT,			*PUSP_CONTEXT;
typedef struct	_USP_PROGDESC_				USP_PROGDESC,			*PUSP_PROGDESC;
typedef struct	_USP_ITERATED_DATA_			USP_ITERATED_DATA,		*PUSP_ITERATED_DATA;
typedef struct	_USP_PRESAMPLED_DATA_		USP_PRESAMPLED_DATA,	*PUSP_PRESAMPLED_DATA;
typedef struct	_USP_TEXSTATE_DATA_			USP_TEXSTATE_DATA,		*PUSP_TEXSTATE_DATA;
typedef struct	_USP_INPUT_DATA_			USP_INPUT_DATA,			*PUSP_INPUT_DATA;
typedef struct	_USP_MOESTATE_				USP_MOESTATE,			*PUSP_MOESTATE;
typedef struct	_USP_INSTDESC_				USP_INSTDESC,			*PUSP_INSTDESC;
typedef struct	_USP_INST_					USP_INST,				*PUSP_INST;
typedef struct	_USP_INSTBLOCK_				USP_INSTBLOCK,			*PUSP_INSTBLOCK;
typedef struct	_USP_LABEL_					USP_LABEL,				*PUSP_LABEL;
typedef struct	_USP_BRANCH_				USP_BRANCH,				*PUSP_BRANCH;
typedef	struct	_USP_TEX_CHAN_INFO_			USP_TEX_CHAN_INFO,		*PUSP_TEX_CHAN_INFO;
typedef struct	_USP_SAMPLE_				USP_SAMPLE,				*PUSP_SAMPLE;
typedef struct	_USP_SAMPLEUNPACK_			USP_SAMPLEUNPACK,		*PUSP_SAMPLEUNPACK;
typedef struct	_USP_RESULTREF_				USP_RESULTREF,			*PUSP_RESULTREF;
typedef struct	_USP_SHADER_				USP_SHADER,				*PUSP_SHADER;
typedef struct	_USP_TEXTURE_WRITE_			USP_TEXTURE_WRITE,		*PUSP_TEXTURE_WRITE;

#endif	/* #ifndef _USP_TYPEDEFS_H_ */
/*****************************************************************************
 End of file (USP_TYPEDEFS.H)
*****************************************************************************/
