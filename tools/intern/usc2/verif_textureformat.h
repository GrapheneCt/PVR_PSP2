/*****************************************************************************
 * Name			: verif_textureformat.h
 * Title		: Interface to verifier
 * Created  	: Nov 2006
 * 
 * Copyright	: 2002 by Imagination Technologies Ltd. All rights reserved.
 *				  No part of this software, either material or conceptual	
 *				  may be copied or distributed, transmitted, transcribed,
 *				  stored in a retrieval system or translated into any	
 *				  human or computer language in any form by any means,
 *				  electronic, mechanical, manual or other-wise, or 
 *				  disclosed to third parties without the express written
 *				  permission of Imagination Technologies Ltd, HomePark
 *				  Industrial Estate, King's Langley, Hertfordshire,
 *				  WD4 8LZ, U.K.
 *
 * Modifications :-
 * $Log: verif_textureformat.h $
 *****************************************************************************/

#ifndef __VERIF_TEXTUREFORMAT_H
#define __VERIF_TEXTUREFORMAT_H

#include "img_types.h"

typedef enum _VERIF_TEXEL_MEMORY_FORMAT
{
	VERIF_TEXEL_MEMORY_FORMAT_UNDEF,
	VERIF_TEXEL_MEMORY_FORMAT_U8,
	VERIF_TEXEL_MEMORY_FORMAT_U88,
	VERIF_TEXEL_MEMORY_FORMAT_U8888,
	VERIF_TEXEL_MEMORY_FORMAT_S8,
	VERIF_TEXEL_MEMORY_FORMAT_S88,
	VERIF_TEXEL_MEMORY_FORMAT_S8888,
	VERIF_TEXEL_MEMORY_FORMAT_U4444,
	VERIF_TEXEL_MEMORY_FORMAT_U565,
	VERIF_TEXEL_MEMORY_FORMAT_U1555,
	VERIF_TEXEL_MEMORY_FORMAT_U8332,
	VERIF_TEXEL_MEMORY_FORMAT_U6S55,
	VERIF_TEXEL_MEMORY_FORMAT_U16,
	VERIF_TEXEL_MEMORY_FORMAT_U1616,
	VERIF_TEXEL_MEMORY_FORMAT_S16,
	VERIF_TEXEL_MEMORY_FORMAT_S1616,
	VERIF_TEXEL_MEMORY_FORMAT_F16,
	VERIF_TEXEL_MEMORY_FORMAT_F1616,
	VERIF_TEXEL_MEMORY_FORMAT_F32,
	VERIF_TEXEL_MEMORY_FORMAT_PVRT2BPP,
	VERIF_TEXEL_MEMORY_FORMAT_PVRT4BPP,
	VERIF_TEXEL_MEMORY_FORMAT_PVRTII2BPP,
	VERIF_TEXEL_MEMORY_FORMAT_PVRTII4BPP,
	VERIF_TEXEL_MEMORY_FORMAT_PVRTIII
} VERIF_TEXEL_MEMORY_FORMAT, *PVERIF_TEXEL_MEMORY_FORMAT;

typedef enum _VERIF_INPUT_TEXTURE_CHANNEL_FORMAT
{
	/* No special conversion for this channel. */
	VERIF_INPUT_TEXTURE_CHANNEL_FORMAT_OTHER,
	/*
		This channel is an 8-bit unsigned integer. Generate an expression representing a conversion
		to a 32-bit unsigned integer.
	*/
	VERIF_INPUT_TEXTURE_CHANNEL_FORMAT_UINT8,
	/*
		This channel is an 8-bit signed integer. Generate an expression representing a conversion
		to a 32-bit signed integer.
	*/
	VERIF_INPUT_TEXTURE_CHANNEL_FORMAT_SINT8,
	/*
		This channel is an 16-bit unsigned integer. Generate an expression representing a conversion
		to a 32-bit unsigned integer.
	*/
	VERIF_INPUT_TEXTURE_CHANNEL_FORMAT_UINT16,
	/*
		This channel is an 16-bit signed integer. Generate an expression representing a conversion
		to a 32-bit signed integer.
	*/
	VERIF_INPUT_TEXTURE_CHANNEL_FORMAT_SINT16,
	/*
		The channel is a 32-bit float without a sign bit. Generate an expression representing
		a conversion to an ordinary 32-bit float.
	*/
	VERIF_INPUT_TEXTURE_CHANNEL_FORMAT_F32_NO_SIGN_BIT
} VERIF_INPUT_TEXTURE_CHANNEL_FORMAT, *PVERIF_INPUT_TEXTURE_CHANNEL_FORMAT;

/*
	Describes the format of a texture to the verifier phase which simulates
	the input program.
*/
typedef struct _VERIF_INPUT_TEXTURE_FORMAT
{
	IMG_UINT32							uSwizzle;
	/*
		Mask of the channels which always have data in the range 0..1.
	*/
	IMG_UINT32							uSaturatedChannelMask;
	/*
		Index of the start of the matrix input constant to use to apply colour space conversion
		to the result of the texture sample.
	*/
	IMG_UINT32							uCSCConst;
	/*
		If TRUE texture lookups on this texture use PCF. The input verifier will include the
		PCF comparison value in the generated expression.
	*/
	IMG_BOOL							bUsePCF;
	/*
		Size of a texel in memory in bytes. Used to scale the source coordinate to the LDBUFF
		instruction.
	*/
	IMG_UINT32							uMemoryTexelSizeInBytes;
	/*
		Formats of the channels in the texture. Used where the input verifier needs to generate
		a special expression to represent the format.
	*/
	VERIF_INPUT_TEXTURE_CHANNEL_FORMAT	aeChannelFormat[4];
} VERIF_INPUT_TEXTURE_FORMAT, *PVERIF_INPUT_TEXTURE_FORMAT;

#define VERIF_OUTPUT_TEXTURE_FORMAT_MAX_CHANNELS	(4)

typedef enum _VERIF_OUTPUT_TEXTURE_FORMAT_SOURCE
{
	VERIF_OUTPUT_TEXTURE_FORMAT_SOURCE_CHAN0,
	VERIF_OUTPUT_TEXTURE_FORMAT_SOURCE_CHAN1,
	VERIF_OUTPUT_TEXTURE_FORMAT_SOURCE_CHAN2,
	VERIF_OUTPUT_TEXTURE_FORMAT_SOURCE_CHAN3,
	VERIF_OUTPUT_TEXTURE_FORMAT_SOURCE_CONSTANT_ZERO,
	VERIF_OUTPUT_TEXTURE_FORMAT_SOURCE_CONSTANT_ONE,
	VERIF_OUTPUT_TEXTURE_FORMAT_SOURCE_UNDEFINED
} VERIF_OUTPUT_TEXTURE_FORMAT_SOURCE, *PVERIF_OUTPUT_TEXTURE_FORMAT_SOURCE;

typedef enum _VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE
{
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_U8 = 0,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_U8_UN,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_S8,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_C10,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_U16,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_S16,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_F16,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_F32,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_UINT8,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_SINT8,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_UINT16,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_SINT16,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_UINT32,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_SINT32,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_U24,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_UNCHANGED,
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE_UNDEFINED
} VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE, *PVERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE;

typedef struct _VERIF_OUTPUT_TEXTURE_FORMAT_CHANNEL
{
	VERIF_OUTPUT_TEXTURE_FORMAT_SOURCE		eSource;
	VERIF_OUTPUT_TEXTURE_FORMAT_DATA_TYPE	eDataType;
} VERIF_OUTPUT_TEXTURE_FORMAT_CHANNEL, *PVERIF_OUTPUT_TEXTURE_FORMAT_CHANNEL; 

typedef struct _VERIF_OUTPUT_TEXTURE_FORMAT_CHUNK
{
	/*
		Count of channels to write when sampling this texture and chunk.
	*/
	IMG_UINT32								uChanCount;

	/*
		If TRUE then if the TAG/TF is unpacking to F16 the red and blue channels are 
		swapped.
	*/
	IMG_BOOL								bF16BGRA;

	/*
		Minimum size of the results written by the hardware to the instruction destination when sampling
		this texture and chunk. 
	*/
	IMG_UINT32								uMinimumResultSizeInBits;

	/*
		Data written by the hardware to the instruction destination when sampling this texture and
		chunk.
	*/
	VERIF_OUTPUT_TEXTURE_FORMAT_CHANNEL		asChannels[VERIF_OUTPUT_TEXTURE_FORMAT_MAX_CHANNELS];
} VERIF_OUTPUT_TEXTURE_FORMAT_CHUNK, *PVERIF_OUTPUT_TEXTURE_FORMAT_CHUNK;

typedef VERIF_OUTPUT_TEXTURE_FORMAT_CHUNK const* PCVERIF_OUTPUT_TEXTURE_FORMAT_CHUNK;

typedef struct _VERIF_OUTPUT_TEXTURE_FORMAT
{
	IMG_UINT32							uMemoryTexelSizeInBytes;
	IMG_UINT32							auChunkOffsetInMemory[UF_MAX_CHUNKS_PER_TEXTURE];
	IMG_UINT32							uSaturatedChannelMask;
	IMG_UINT32							uNumChunks;
	VERIF_OUTPUT_TEXTURE_FORMAT_CHUNK	asChunks[UF_MAX_CHUNKS_PER_TEXTURE];
} VERIF_OUTPUT_TEXTURE_FORMAT, *PVERIF_OUTPUT_TEXTURE_FORMAT;

typedef VERIF_OUTPUT_TEXTURE_FORMAT const* PCVERIF_OUTPUT_TEXTURE_FORMAT;

#endif /* __VERIF_TEXTUREFORMAT_H */

/* EOF */
