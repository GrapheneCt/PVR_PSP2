/**************************************************************************
 * Name         : esbinshaderinternal.h
 * Created      : 6/07/2006
 *
 * Copyright    : 2000-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: esbinshaderinternal.h $
 */

#ifndef _ESBINSHADERINTERNAL_H_
#define _ESBINSHADERINTERNAL_H_

/*
 *   Terminology and main concepts:
 *
 *       - Binary Shader Format is the language chosen by IMG to support 
 *         glShaderBinary() in OpenGL ES 2.x in the SGX family of chips.
 *
 *       - A Binary is a shader written in the Binary Shader Format.
 *         That is, a Binary is a valid input to glShaderBinary().
 *         One shader in source form translates to one Binary.
 *
 *         The Binary Shader Format has a builtin mechanism to be
 *         backwards compatible in case a new software or hardware version 
 *         requires changes in the Format. Binaries may contain several
 *         different encodings of the same shader to target different software
 *         or hardware variants. They are called Revisions of the 
 *         shader. New versions of the Binary Shader format can only 
 *         extend the previous versions by adding new ways of encoding a 
 *         Revision thus ensuring backwards compatibility. 
 *         See the next item for the formal definition.
 *
 *       - A Revision Format is a language used to encode a Revision. Each
 *         Binary Shader Format version extends the previous by adding one or
 *         more Revision Formats.
 *
 *         The first data structure found in every Revision of the shader
 *         is SGXBS_RevisionHeader. This structure specififes the Revision
 *         format in which the Revision is written and its length in 32-bit words.
 *         The driver must quietly ignore all Revisions written in versions it doesn't 
 *         understand.
 *
 *
 *   Explanation of the Binary Shader Format version 1.0:
 *   
 *   Each Binary is stored (either in a file or in memory) as a sequence of 
 *   structs. Each struct is defined using big-endian integers and IEEE 754
 *   floats. The use of C enums is forbidden as their size is compiler-
 *   -dependent (see ISO C99: 6.4.4.3, 6.7.2.2). 8-bit, 16-bit or 32-bit
 *   unsigned integers are used instead. Booleans are stored as 8-bit or 16-bit
 *   unsigned integers. A value of 0 is logically FALSE and any other is TRUE,
 *   although it is strongly recommended to use the value 1.
 *
 *   The internal structs are essentialy subsets of those found in glsl.h, usp.h and
 *   ic2uf.h hence the lack of comments explaining the meaning of each
 *   variable. Their names have been prefixed with 'SGXBS_' to avoid
 *   collisions. Read the comments in the original structs to understand the
 *   meaning of each variable.
 *
 *   The software version flag for the Revision Format 1.0 is 0x0001.
 *
 * 
 *   Informal diagram of the structure of a Binary in any Binary Shader Format:
 *
 *   +-------------------------------------------------------------+ 
 *   |                                                             | \  
 *   |              packed struct SGXBS_BinaryHeader               |  | 
 *   |                                                             |  | 
 *   |      +-----------------------------------------------+      |  |
 *   |      | u32BinaryHeaderMagic                          |      |  |
 *   |      +-----------------------------------------------+      |  |
 *   |      | u32Hash                                       |      |  |
 *   |      +-----------------------------------------------+      |  |
 *   |                                                             |  |
 *   +-------------------------------------------------------------+  |
 *   |                                                             |  |
 *   |             packed struct SGXBS_RevisionHeader              |   > These two structs must always be 
 *   |                                                             |  |  at the very beginning.
 *   |      +-----------------------------------------------+      |  |  
 *   |      | u16SoftwareVersion                            |      |  |
 *   |      +-----------------------------------------------+      |  |  
 *   |      | u16Core (major SGX version)                   |      |  |
 *   |      +-----------------------------------------------+      |  |  
 *   |      | u16CoreRevision (minor SGX revision)          |      |  |
 *   |      +-----------------------------------------------+      |  |  
 *   |      | u16ReservedA (must be zero)                   |      |  |
 *   |      +-----------------------------------------------+      |  |
 *   |      | u32ProjectVersionHash (eurasiacon.pj version) |      |  |
 *   |      +-----------------------------------------------+      |  |  
 *   |      | u32CompiledGLSLVersion                        |      |  |
 *   |      +-----------------------------------------------+      |  |  
 *   |      | u32USPPCShaderVersion                         |      |  |
 *   |      +-----------------------------------------------+      |  |
 *   |      | u32LengthInBytes                              |      |  |
 *   |      +-----------------------------------------------+      |  |  
 *   |                                                             | /
 *   +- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -+
 *   |                                                             | \
 *   |    Main Revision data. The Revision Format and length of    |  |  Every Binary has at least 
 *   |    this data must be according to the above RevisionHeader. |  |  one Revision.
 *   |                                                             | /
 *   +-------------------------------------------------------------+ 
 *   |                                                             | \
 *   |             packed struct SGXBS_RevisionHeader              |  |
 *   |                                                             |  |
 *   +- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -+  |
 *   |                                                             |  |
 *   |    Revision data. The Revision Format and length of this    |  |
 *   |    data must be according to the previous RevisionHeader.   |   > These are optional.
 *   |                                                             |  |
 *   +-------------------------------------------------------------+  |
 *   |                                                             |  |
 *   |                   ...more Revisions...                      |  |
 *   |                                                             | /
 *   +-------------------------------------------------------------+ 
 *
 *   Note that in version 1.0 of the Binary Shader Format, each binary 
 *   contains one and only one Revision of the shader since there is only one 
 *   Revision Format in the specification.
 *
 *   Formal specification of the structs above:
 *
 *      Header of the binary format. This is the first data stored in it and is
 *      used to verify the integrity of the data.
 *
 *         packed struct BinaryHeader:
 * 
 *            # Magic number to make it easy for third parties to identify that the data is an IMG SGX binary.
 *            # It must be equal to the macro SGXBS_HEADER_MAGIC_NUMBER defined below.
 *            IMG_UINT32                      u32BinaryHeaderMagic;
 *
 *            # Hash of the rest of the Binary (starting just after the hash itself).
 *            # If it doesn't match, the Binary is corrupt.
 *            IMG_UINT32                      u32Hash;
 *
 *            # ...a Revision follows here...
 *
 *         end struct
 *
 *
 *      Version of the format in which this revision is stored. The driver 
 *      must quietly ignore revisions written in format versions unknown to 
 *      it and skip to the next revision. If all revisions in the binary are
 *      written in formats unknown to the driver, it should issue an error.
 *       
 *      The version is expressed in terms of:
 *          a) The software version. May change if, for example, a new version
 *             of GLSL is released.
 *          b) The hardware core version. That is, 520, 530, 535, 545, etc.
 *          c) The hardware core revision. That is, bug fixes of the previous.
 *          d) A 16-bit reserved word for future use. It must be zeroed.
 *          e) A hash of the eurasiacon.pj revision used when building this compiler.
 *          f) u32CompiledGLSLVersion is the version number of the interface of the pre-compiled GLSL (GLSLCompiledUniflexProgram).
 *			g) u32USPPCShaderVersion is the version number of the UniPatch input (USP_PC_SHADER).
 *      
 *         packed struct RevisionHeader:
 *
 *            IMG_UINT16                      u16SoftwareVersion;
 *            IMG_UINT16                      u16Core;
 *            IMG_UINT16                      u16CoreRevision;
 *            IMG_UINT32                      u16ReservedA;
 *            IMG_UINT16                      u32ProjectVersionHash;
 *            IMG_UINT32                      u32CompiledGLSLVersion;
 *            IMG_UINT32                      u32USPPCShaderVersion;
 *
 *            # Length of the revision in bytes excluding this header.
 *            IMG_UINT32                      u32LengthInBytes;
 * 
 *            # ...the RevisionBody follows here...
 *
 *         end struct
 *   
 *   
 *   --------------------------------------------
 *   --- Explanation of the Revision Format 1 ---
 *   --------------------------------------------
 *   
 *   The flags in the Revision Header must be:
 *
 *      u16SoftwareVersion      = SGXBS_SOFTWARE_VERSION_1
 *      u16CoreVersion          = SGXBS_CORE_5xx
 *      u16Revision             = SGXBS_CORE_REVISION_1xx
 *		u32CompiledGLSLVersion	= GLSL_COMPILED_UNIFLEX_INTERFACE_VER
 *		u32USPPCShaderVersion	= USP_PC_SHADER_VER
 *   
 *   Variable-length arrays are represented with this structure to know the
 *   number of elements in it.
 *
 *      packed struct ArrayHeader:
 *
 *         # Number of elements (as opposed to bytes) in the array.
 *         IMG_UINT16                u16Elements;
 *
 *         # ...the actual data of the array follows here...
 *
 *      end struct
 * 
 *   For example, the memory layout of the array of integers (0x123456, 0x56789, 0x890)
 *   is: (0x0003, 0x123456, 0x56789, 0x890)
 *
 *
 *   The Revision Body matches the following structure (see GLSLCompiledUniflexProgram):
 *
 *      packed struct RevisionBody_1:
 *    
 *         # --- From GLSLCompiledUniflexProgram ---
 *         #
 *         GLSLShaderType                  eShaderType;
 *         GLSLShaderFlags                 eShaderFlags;
 *         IMG_UINT32                      u32ReservedForFutureUse;
 *
 *         # --- From GLSLUniFlexCode ---
 *         #
 *         GLSLVaryingMask                 eActiveVaryingMask;
 *         IMG_UINT8                       au8TexCoordDims[SGXBS_NUM_TC_REGISTERS];
 *         GLSLPrecisionQualifier          aeTexCoordPrecisions[SGXBS_NUM_TC_REGISTERS];
 *
 *         # --- The UniPatch input code ---
 *         #
 *         IMG_UINT32                      u32UniPatchInputSizeInBytes
 *         # ...the UniPatch opaque data blocks follow here (see struct _USP_PC_SHADER_)...
 *
 *         # --- The UniPatch input code for Multi Sampling Anti-Aliasing with translucency ---
 *         # --- IMPORTANT: This field only appears in FRAGMENT shaders and is missing in vertex shaders. ---
 *         #
 *         IMG_UINT32                      u32UniPatchMSAAInputSizeInBytes
 *         # ...the UniPatch opaque data blocks follow here (see struct _USP_PC_SHADER_)...
 *         
 *         # --- From GLSLBindingSymbolList ---
 *         #
 *         # Variable-length array of 32-bit words storing the value
 *         # of the constants used in this shader.
 *         #
 *         ArrayHeader                     sConstantDataArray;
 *
 *         # ...the array data follows here...
 *
 *         # Variable-length array of type BindingSymbol. Each symbol
 *         # in the shader has an entry in this array.
 *         #
 *         ArrayHeader                     sBindingsArray;
 *
 *         # ...the array data follows here...
 *
 *      end struct
 *
 *
 *   Definition of the BindingSymbol structure used above:
 *
 *      packed struct BindingSymbol:
 *
 *         # ...a NULL-terminated array of IMG_CHAR follows here storing the name of the symbol...
 *
 *         GLSLBuiltInVariableID           eBIVariableID;
 *         GLSLTypeSpecifier               eTypeSpecifier;
 *         GLSLTypeQualifier               eTypeQualifier;
 *         GLSLVaryingModifierFlags        eVaryingModifierFlags;
 *         IMG_UINT16                      u16ActiveArraySize;
 *         IMG_UINT16                      u16DeclaredArraySize;
 *
 *         GLSLHWRegType                   eRegType;
 *         union
 *         {
 *             IMG_UINT16                  u16BaseComp;
 *             IMG_UINT16                  u16TextureUnit;
 *         } u;
 *         IMG_UINT8                       u8CompAllocCount;
 *         IMG_UINT16                      u16CompUseMask;
 *
 *         # If this symbol is a struct, then the following is a variable-length
 *         # array of type BindingSymbol storing the members of the struct.
 *         #
 *         ArrayHeader                     sBaseTypeMembersArray;
 *
 *         # ...the array data follows here...
 *
 *      end struct
 *
 *
 *   Definition of the enums used in RevisionBody_1 and BindingSymbol:
 *
 *       Former enums converted into unsigned integers.
 *       Whenever in this document we refer to these enums, they must be replaced with the integers in this table.
 *       (Sorted in alphabetical order)
 *
 *          GLSLHWRegType            -> IMG_UINT8
 *          GLSLBuiltInVariableID    -> IMG_UINT16
 *          GLSLShaderFlags          -> IMG_UINT16
 *          GLSLShaderType           -> IMG_UINT16
 *          GLSLTypeQualifier        -> IMG_UINT8
 *          GLSLTypeSpecifier        -> IMG_UINT8
 *          GLSLPrecisionQualifier   -> IMG_UINT8
 *          GLSLVaryingMask          -> IMG_UINT32
 *          GLSLVaryingModifierFlags -> IMG_UINT8
 *
 *   --------------------------------------------
 *   --- End of Revision Format 1             ---
 *   --------------------------------------------
 */

/*
 * Constants used for the Revision Header.
 */
#define SGXBS_HEADER_MAGIC_NUMBER                      ((IMG_UINT32) 0x38B4FA10)

#define SGXBS_SOFTWARE_VERSION_1                       ((IMG_UINT16) 0x0001U)

#define SGXBS_CORE_520                                 ((IMG_UINT16) 0x0520U)
#define SGXBS_CORE_530                                 ((IMG_UINT16) 0x0530U)
#define SGXBS_CORE_531                                 ((IMG_UINT16) 0x0531U)
#define SGXBS_CORE_535                                 ((IMG_UINT16) 0x0535U)
#define SGXBS_CORE_540                                 ((IMG_UINT16) 0x0540U)
#define SGXBS_CORE_541								   ((IMG_UINT16) 0x0541U)
#define SGXBS_CORE_543								   ((IMG_UINT16) 0x0543U)
#define SGXBS_CORE_544								   ((IMG_UINT16) 0x0544U)
#define SGXBS_CORE_545                                 ((IMG_UINT16) 0x0545U)
#define SGXBS_CORE_554                                 ((IMG_UINT16) 0x0554U)

#define SGXBS_CORE_REVISION_100                        ((IMG_UINT16) 0x0100U)
#define SGXBS_CORE_REVISION_101                        ((IMG_UINT16) 0x0101U)
#define SGXBS_CORE_REVISION_102                        ((IMG_UINT16) 0x0102U)
#define SGXBS_CORE_REVISION_103                        ((IMG_UINT16) 0x0103U)
#define SGXBS_CORE_REVISION_104                        ((IMG_UINT16) 0x0104U)
#define SGXBS_CORE_REVISION_105                        ((IMG_UINT16) 0x0105U)
#define SGXBS_CORE_REVISION_106                        ((IMG_UINT16) 0x0106U)
#define SGXBS_CORE_REVISION_107                        ((IMG_UINT16) 0x0107U)
#define SGXBS_CORE_REVISION_108                        ((IMG_UINT16) 0x0108U)
#define SGXBS_CORE_REVISION_109                        ((IMG_UINT16) 0x0109U)
#define SGXBS_CORE_REVISION_1012                       ((IMG_UINT16) 0x1012U)
#define SGXBS_CORE_REVISION_1013                       ((IMG_UINT16) 0x1013U)
#define SGXBS_CORE_REVISION_10131                      ((IMG_UINT16) 0x1131U)
#define SGXBS_CORE_REVISION_1014                       ((IMG_UINT16) 0x1014U)
#define SGXBS_CORE_REVISION_110                        ((IMG_UINT16) 0x0110U)
#define SGXBS_CORE_REVISION_111                        ((IMG_UINT16) 0x0111U)
#define SGXBS_CORE_REVISION_1111					   ((IMG_UINT16) 0x1111U)
#define SGXBS_CORE_REVISION_112                        ((IMG_UINT16) 0x0112U)
#define SGXBS_CORE_REVISION_113                        ((IMG_UINT16) 0x0113U)
#define SGXBS_CORE_REVISION_114                        ((IMG_UINT16) 0x0114U)
#define SGXBS_CORE_REVISION_115                        ((IMG_UINT16) 0x0115U)
#define SGXBS_CORE_REVISION_116                        ((IMG_UINT16) 0x0116U)
#define SGXBS_CORE_REVISION_120                        ((IMG_UINT16) 0x0120U)
#define SGXBS_CORE_REVISION_121                        ((IMG_UINT16) 0x0121U)
#define SGXBS_CORE_REVISION_122                        ((IMG_UINT16) 0x0122U)
#define SGXBS_CORE_REVISION_1221                       ((IMG_UINT16) 0x1221U)
#define SGXBS_CORE_REVISION_123                        ((IMG_UINT16) 0x0123U)
#define SGXBS_CORE_REVISION_124                        ((IMG_UINT16) 0x0124U)
#define SGXBS_CORE_REVISION_125                        ((IMG_UINT16) 0x0125U)
#define SGXBS_CORE_REVISION_1251                       ((IMG_UINT16) 0x1251U)
#define SGXBS_CORE_REVISION_126                        ((IMG_UINT16) 0x0126U)
#define SGXBS_CORE_REVISION_130                        ((IMG_UINT16) 0x0130U)
#define SGXBS_CORE_REVISION_140                        ((IMG_UINT16) 0x0140U)
#define SGXBS_CORE_REVISION_1401                       ((IMG_UINT16) 0x1401U)
#define SGXBS_CORE_REVISION_141                        ((IMG_UINT16) 0x0141U)
#define SGXBS_CORE_REVISION_142                        ((IMG_UINT16) 0x0142U)
#define SGXBS_CORE_REVISION_211                        ((IMG_UINT16) 0x0211U)
#define SGXBS_CORE_REVISION_2111                       ((IMG_UINT16) 0x2111U)
#define SGXBS_CORE_REVISION_213                        ((IMG_UINT16) 0x0213U)
#define SGXBS_CORE_REVISION_216                        ((IMG_UINT16) 0x0216U)
#define SGXBS_CORE_REVISION_302                        ((IMG_UINT16) 0x0302U)
#define SGXBS_CORE_REVISION_303                        ((IMG_UINT16) 0x0303U)


/*
 * Hash of a sequence of bytes.
 */
typedef struct SGXBS_HashTAG
{
	IMG_UINT32                      u32Hash;

} SGXBS_Hash;

/*
 * Minimum length of any valid Binary in bytes. If the length is shorter,
 * the Binary is necessarily invalid.
 */
#define SGXBS_MINIMUM_VALID_BINARY_SIZE                (SGXBS_BinaryHeader_size + SGXBS_RevisionHeader_size)

#define SGXBS_Hash_size            4

#define SGXBS_BinaryHeader_size   (4+SGXBS_Hash_size)

#define SGXBS_RevisionHeader_size  20



/* ------------------------------------------------------------------------- */
/*                      REVISION FORMAT 1 CONSTANTS                          */
/* ------------------------------------------------------------------------- */

/*
 * Constants re-declared here to ensure they'll never change.
 */
#define SGXBS_NUM_TC_REGISTERS      10

#if SGXBS_NUM_TC_REGISTERS != NUM_TC_REGISTERS
#error "NUM_TC_REGISTERS has changed its value. The binary interface is broken. Need to create a new Revision."
#endif


/* ------------------------------------------------------------------ */
/*        Functions shared in the coder and the decoder               */
/* ------------------------------------------------------------------ */


/* The hash salt is used to break compatibility with previous versions of the binary format.
   As we approach our release date, the salt will be changed to mark the point in which we
   start providing binary compatibility (if any).
*/
#define SGXBS_HASH_SALT 0x8001

/*
 * This is adapted from Paul Hsieh's hash function.
 * The original is freely available at http://www.azillionmonkeys.com/qed/hash.html
 */
static SGXBS_Hash SGXBS_ComputeHash(const IMG_UINT8* pu8Sequence, IMG_UINT32 u32LengthInBytes)
{
	SGXBS_Hash sResult;
	IMG_UINT32 hash = u32LengthInBytes, tmp, rem;

    rem                = u32LengthInBytes & 0x3;
    u32LengthInBytes >>= 2;

    /* Main loop */
    for (;u32LengthInBytes; u32LengthInBytes--)
	{
        hash        +=  (pu8Sequence[1] <<  8) |  pu8Sequence[0];
        tmp          = ((pu8Sequence[3] << 19) | (pu8Sequence[2] << 11)) ^ hash;
        hash         = (hash << 16) ^ tmp;
        pu8Sequence += 2*sizeof (IMG_UINT16);
        hash        += hash >> 11;
    }

	/* Handle end cases */
	switch (rem)
	{
        case 1: hash += *pu8Sequence;
                hash ^= hash << 10;
                hash += hash >> 1;
				break;
        case 2: hash += (pu8Sequence[1] << 8)  | pu8Sequence[0];
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 3: hash += (pu8Sequence[1] << 8)  | pu8Sequence[0];
                hash ^= hash << 16;
                hash ^= pu8Sequence[2] << 18;
                hash += hash >> 11;
                break;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash <<  3;
    hash += hash >>  5;
    hash ^= hash <<  4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >>  6;

	sResult.u32Hash = hash + SGXBS_HASH_SALT;

	return sResult;
}


/*
 * Tests that the sizeof the internal structures used in the shader match the 
 * specification. If they don't match, it means that the compiler is not packing 
 * the data in the structures correctly.
 * Returns IMG_TRUE if successful, or IMG_FALSE otherwise.
 */
static IMG_BOOL SGXBS_TestBinaryShaderInterface(IMG_VOID)
{
	IMG_BOOL   bResult = IMG_TRUE;

	/*
	 * Test first the size of basic data types.
	 */


/* The sizes of the data types may not be the expected values on some platforms. */
/* PRQA S 3201 ++ */
#define SGXBS_CHECK_SIZEOF(basictype, expectedsize)  \
	if(sizeof(basictype) != expectedsize)            \
	{                                                \
		bResult = IMG_FALSE;                         \
	}                                               \

	SGXBS_CHECK_SIZEOF(IMG_CHAR,   1)
	SGXBS_CHECK_SIZEOF(IMG_UINT8,  1)
	SGXBS_CHECK_SIZEOF(IMG_INT16,  2)
	SGXBS_CHECK_SIZEOF(IMG_UINT16, 2)
	SGXBS_CHECK_SIZEOF(IMG_INT32,  4)
	SGXBS_CHECK_SIZEOF(IMG_UINT32, 4)
	SGXBS_CHECK_SIZEOF(IMG_FLOAT,  4)

#undef SGXBS_CHECK_SIZEOF
/* PRQA S 3201 -- */


	return bResult;
}



#endif /* !defined _ESBINSHADERINTERNAL_H_ */
