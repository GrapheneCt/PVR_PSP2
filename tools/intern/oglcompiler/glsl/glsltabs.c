/******************************************************************************
 * Name		 : glsltabs.c
 * Author	   : James McCarthy
 * Created	  : 16/05/2006
 *
 * Copyright	: 2000-2007 by Imagination Technologies Limited.
 *			  : All rights reserved. No part of this software, either
 *			  : material or conceptual may be copied or distributed,
 *			  : transmitted, transcribed, stored in a retrieval system or
 *			  : translated into any human or computer language in any form
 *			  : by any means, electronic, mechanical, manual or otherwise,
 *			  : or disclosed to third parties without the express written
 *			  : permission of Imagination Technologies Limited,
 *			  : Home Park Estate, Kings Langley, Hertfordshire,
 *			  : WD4 8LZ, U.K.
 *
 * Platform	 : ANSI
 *
 * Modifications:-
 * $Log: glsltabs.c $
 *****************************************************************************/

#include "glsl.h"
#include "glsltree.h"



IMG_INTERNAL const GLSLTypeSpecifierInfo asGLSLTypeSpecifierInfoTable[GLSLTS_NUM_TYPES] = 
{
	/* GLSLTS_							Elements	Dimension	Size											Indexed		BaseType						FullDesc									Desc							ColumnsCM	RowsCM		IsNumber */
	{GLSLTS_(INVALID)					0,			0,			0,												0,			GLSLTS_INVALID,					"invalid",									"invalid",						0,			0,			IMG_FALSE,		},
	{GLSLTS_(VOID)						0,			0,			0,												0,			GLSLTS_VOID ,					"void",										"void",							0,			0,			IMG_FALSE,		},
	{GLSLTS_(FLOAT)						1,			1,			1  * sizeof(IMG_FLOAT),	/* GLSLTS_FLOAT */		0,			GLSLTS_FLOAT,					"float",									"float",						1,			1,			IMG_TRUE,		},
	{GLSLTS_(VEC2)						2,			2,			2  * sizeof(IMG_FLOAT),	/* GLSLTS_VEC2 */		1,			GLSLTS_FLOAT,					"2-component vector of float",				"vec2",							1,			2,			IMG_TRUE,		},
	{GLSLTS_(VEC3)						3,			3,			3  * sizeof(IMG_FLOAT),	/* GLSLTS_VEC3 */		1,			GLSLTS_FLOAT,					"3-component vector of float",				"vec3",							1,			3,			IMG_TRUE,		},
	{GLSLTS_(VEC4)						4,			4,			4  * sizeof(IMG_FLOAT),	/* GLSLTS_VEC4 */		1,			GLSLTS_FLOAT,					"4-component vector of float",				"vec4",							1,			4,			IMG_TRUE,		},
	{GLSLTS_(INT)						1,			1,			1  * sizeof(IMG_UINT32),/* GLSLTS_INT */		0,			GLSLTS_INT,						"int",										"int",							1,			1,			IMG_TRUE,		},
	{GLSLTS_(IVEC2)						2,			2,			2  * sizeof(IMG_UINT32),/* GLSLTS_IVEC2 */		1,			GLSLTS_INT,						"2-component vector of int",				"ivec2",						1,			2,			IMG_TRUE,		},
	{GLSLTS_(IVEC3)						3,			3,			3  * sizeof(IMG_UINT32),/* GLSLTS_IVEC3 */		1,			GLSLTS_INT,						"3-component vector of int",				"ivec3",						1,			3,			IMG_TRUE,		},
	{GLSLTS_(IVEC4)						4,			4,			4  * sizeof(IMG_UINT32),/* GLSLTS_IVEC4 */		1,			GLSLTS_INT,						"4-component vector of int",				"ivec4",						1,			4,			IMG_TRUE,		},
	{GLSLTS_(BOOL)						1,			1,			1  * sizeof(IMG_BOOL),	/* GLSLTS_BOOL */		0,			GLSLTS_BOOL,					"bool",										"bool",							1,			1,			IMG_FALSE,		},
	{GLSLTS_(BVEC2)						2,			2,			2  * sizeof(IMG_BOOL),	/* GLSLTS_BVEC2 */		1,			GLSLTS_BOOL,					"2-component vector of bool",				"bvec2",						1,			2,			IMG_FALSE,		},
	{GLSLTS_(BVEC3)						3,			3,			3  * sizeof(IMG_BOOL),	/* GLSLTS_BVEC3 */		1,			GLSLTS_BOOL,					"3-component vector of bool",				"bvec3",						1,			3,			IMG_FALSE,		},
	{GLSLTS_(BVEC4)						4,			4,			4  * sizeof(IMG_BOOL),	/* GLSLTS_BVEC4 */		1,			GLSLTS_BOOL,					"4-component vector of bool",				"bvec4",						1,			4,			IMG_FALSE,		},
	{GLSLTS_(MAT2X2)					4,			2,			4  * sizeof(IMG_FLOAT),	/* GLSLTS_MAT2X2 */		2,			GLSLTS_FLOAT,					"2X2 matrix of float",						"mat2x2",						2,			2,			IMG_TRUE,		},
	{GLSLTS_(MAT2X3)					6,			2,			6  * sizeof(IMG_FLOAT),	/* GLSLTS_MAT2X3 */		3,			GLSLTS_FLOAT,					"2X3 matrix of float",						"mat2x3",						2,			3,			IMG_TRUE,		},
	{GLSLTS_(MAT2X4)					8,			2,			8  * sizeof(IMG_FLOAT),	/* GLSLTS_MAT2X4 */		4,			GLSLTS_FLOAT,					"2X4 matrix of float",						"mat2x4",						2,			4,			IMG_TRUE,		},
	{GLSLTS_(MAT3X2)					6,			3,			6  * sizeof(IMG_FLOAT),	/* GLSLTS_MAT3X2 */		2,			GLSLTS_FLOAT,					"3X2 matrix of float",						"mat3x2",						3,			2,			IMG_TRUE,		},
	{GLSLTS_(MAT3X3)					9,			3,			9  * sizeof(IMG_FLOAT),	/* GLSLTS_MAT3X3 */		3,			GLSLTS_FLOAT,					"3X3 matrix of float",						"mat3X3",						3,			3,			IMG_TRUE,		},
	{GLSLTS_(MAT3X4)					12,			3,			12 * sizeof(IMG_FLOAT),	/* GLSLTS_MAT3X4 */		4,			GLSLTS_FLOAT,					"3X4 matrix of float",						"mat3X4",						3,			4,			IMG_TRUE,		},
	{GLSLTS_(MAT4X2)					8,			4,			8  * sizeof(IMG_FLOAT),	/* GLSLTS_MAT4X2*/		2,			GLSLTS_FLOAT,					"4X2 matrix of float",						"mat4X2",						4,			2,			IMG_TRUE,		},
	{GLSLTS_(MAT4X3)					12,			4,			12 * sizeof(IMG_FLOAT),	/* GLSLTS_MAT4X3*/		3,			GLSLTS_FLOAT,					"4X3 matrix of float",						"mat4X3",						4,			3,			IMG_TRUE,		},
	{GLSLTS_(MAT4X4)					16,			4,			16 * sizeof(IMG_FLOAT),	/* GLSLTS_MAT4X4*/		4,			GLSLTS_FLOAT,					"4X4 matrix of float",						"mat4X4",						4,			4,			IMG_TRUE,		},	
	/* For sampler types, the dimension refers to the texture coordinate required by the UFOP_LD instruction,
	   and the element value is the same but excludes array and cubemap coordinates (used for textureSize). */
	{GLSLTS_(SAMPLER1D)					1,			1,			0,												0,			GLSLTS_SAMPLER1D,				"sampler1D",								"sampler1D",					0,			0,			IMG_FALSE,		},
	{GLSLTS_(SAMPLER2D)					2,			2,			0,												0,			GLSLTS_SAMPLER2D,				"sampler2D",								"sampler2D",					0,			0,			IMG_FALSE,		},
	{GLSLTS_(SAMPLER3D)					3,			3,			0,												0,			GLSLTS_SAMPLER3D,				"sampler3D",								"sampler3D",					0,			0,			IMG_FALSE,		},
	{GLSLTS_(SAMPLERCUBE)				2,			3,			0,												0,			GLSLTS_SAMPLERCUBE,				"samplerCube",								"samplerCube",					0,			0,			IMG_FALSE,		},	
	{GLSLTS_(SAMPLER1DSHADOW)			1,			1,			0,												0,			GLSLTS_SAMPLER1DSHADOW,			"sampler1Dshadow",							"sampler1Dshadow",				0,			0,			IMG_FALSE,		},
	{GLSLTS_(SAMPLER2DSHADOW)			2,			2,			0,												0,			GLSLTS_SAMPLER2DSHADOW,			"sampler2Dshadow",							"sampler2Dshadow",				0,			0,			IMG_FALSE,		},	
#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
	{GLSLTS_(SAMPLER2DRECT)				2,			2,			0,												0,			GLSLTS_SAMPLER2DRECT,			"sampler2DRect",							"sampler2DRect",				0,			0,			IMG_FALSE,		},
	{GLSLTS_(SAMPLER2DRECTSHADOW)		2,			2,			0,												0,			GLSLTS_SAMPLER2DRECTSHADOW,		"sampler2DRectshadow",						"sampler2DRectshadow",			0,			0,			IMG_FALSE,		},	
#endif
	{GLSLTS_(SAMPLERSTREAM)				2,			2,			0,												0,			GLSLTS_SAMPLERSTREAM,			"samplerstream",							"samplerstream",				0,			0,			IMG_FALSE,		},
	{GLSLTS_(SAMPLEREXTERNAL)			2,			2,			0,												0,			GLSLTS_SAMPLEREXTERNAL,			"samplerexternal",							"samplerexternal",				0,			0,			IMG_FALSE,		},
	{GLSLTS_(STRUCT)					0,			0,			0,												0,			GLSLTS_STRUCT,					"struct",									"struct",						0,			0,			IMG_FALSE,		},
};

IMG_INTERNAL const IMG_CHAR* const apszGLSLTypeQualifierFullDescTable[GLSLTQ_NUM] = {
	"invalid",				/* GLSLTQ_INVALID */
	"",						/* GLSLTQ_TEMP */
	"const",				/* GLSLTQ_CONST */
	"uniform",				/* GLSLTQ_UNIFORM */
	"vertex in/attribute",	/* GLSLTQ_VERTEX_IN */
	"vertex out/varying",	/* GLSLTQ_VERTEX_OUT */
	"fragment in/varying",	/* GLSLTQ_FRAGMENT_IN */
	"fragment out"			/* GLSLTQ_FRAGMENT_OUT */
};

IMG_INTERNAL const IMG_CHAR* const GLSLPrecisionQualifierFullDescTable[GLSLPRECQ_HIGH + 1] = {
	"unknown",
	"lowp",
	"mediump",
	"highp"
};

IMG_INTERNAL const IMG_CHAR* const GLSLParameterQualifierFullDescTable[GLSLPQ_INOUT + 1] = {
	"invalid",
	"in",
	"out",
	"inout"};

IMG_INTERNAL const IMG_CHAR* const GLSLLValueStatusFullDescTable[GLSLLV_L_VALUE + 1] = {
	"non-l-value",
	"non-l-value(ds)",
	"l-value"};

IMG_INTERNAL const IMG_CHAR* const GLSLArrayStatusFullDescTable[GLSLAS_ARRAY_SIZE_FIXED + 1] = {
	"non-array",
	"array size non fixed",
	"array size fixed"};



/******************************************************************************
 End of file (glsltabs.c)
******************************************************************************/
