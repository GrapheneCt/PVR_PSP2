/******************************************************************************
 * Name		 : glsl.h
 * Author		: James McCarthy
 * Created	  : 29/07/2004
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
 * $Log: glsl.h $
 *****************************************************************************/
#ifndef __gl_glsl_h_
#define __gl_glsl_h_

/* Forward struct declaration */
struct GLSLCompilerPrivateDataTAG;
typedef struct GLSLCompilerPrivateDataTAG GLSLCompilerPrivateData;

#include "img_types.h"
#include "usc.h"
#include "errorlog.h"

#include <stdio.h>

#if defined(GLSL_ES)

	#define GLSL_NUM_VERSIONS_SUPPORTED			1
	#define GLSL_DEFAULT_VERSION_SUPPORT		  100

#else

	#define GLSL_NATIVE_INTEGERS	/* Native integers are needed by GLSL 1.30, because it supports bitwise operations. */

		#define GLSL_NUM_VERSIONS_SUPPORTED	 3

	#define GLSL_DEFAULT_VERSION_SUPPORT	110

#endif


#ifdef COMPACT_MEMORY_MODEL

/* IMPORTANT - If you want to modify this value look at GLSLFullySpecifiedType to change your mind */
#define GLSL_NUM_BITS_FOR_SYMBOL_IDS 16

#else

#define GLSL_NUM_BITS_FOR_SYMBOL_IDS 32

#endif

#define GLSL_COMPILED_UNIFLEX_INTERFACE_VER ((0x0000 << 16) | (0x0002 << 0))

#define REG_COMPONENTS			4
#define NUM_TC_REGISTERS		10
#define USE_INTERGER_COLORS		1

typedef enum GLSLLogFilesTAG
{
	GLSLLF_NOT_LOG								= 0,
	GLSLLF_LOG_ICODE_ONLY						= 1,
	GLSLLF_LOG_ALL								= 2,
} GLSLLogFiles;
	
typedef enum GLSLProgramTypeTAG
{
	GLSLPT_VERTEX								= 0x00000000,
	GLSLPT_FRAGMENT								= 0x00000001,
} GLSLProgramType;

typedef enum GLSLExtensionTAG
{
	GLSLEXT_NONE								= 0x00000000,
	GLSLEXT_OES_TEXTURE_3D						= 0x00000001, // GL_OES_texture_3D
	GLSLEXT_OES_STANDARD_NOISE					= 0x00000002, // GL_OES_standard_noise"
	GLSLEXT_OES_STANDARD_DERIVATIVES			= 0x00000004, // GL_OES_standard_derivatives
	GLSLEXT_OES_INVARIANTALL					= 0x00000008, // #pragma STDGL invariant(all)
	GLSLEXT_IMG_PRECISION						= 0x00000010, // GL_IMG_precision
	GLSLEXT_IMG_TEXTURE_STREAM					= 0x00000020, // GL_IMG_texture_stream2
	GLSLEXT_EXT_TEXTURE_LOD						= 0x00000040, // GL_EXT_shader_texture_lod
	GLSLEXT_OES_TEXTURE_EXTERNAL				= 0x00000080, // GL_OES_texture_external

#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
	GLSLEXT_ARB_TEXTURE_RECTANGLE				= 0x00000080, // GL_ARB_texture_rectangle
#endif

	GLSLEXT_FORCE_I32							= 0x7FFFFFFF
} GLSLExtension;

#if defined(GLSL_ES)
#define GLSLEXT_DEFAULT_EXTENSIONS GLSLEXT_IMG_PRECISION
#else
#define GLSLEXT_DEFAULT_EXTENSIONS GLSLEXT_NONE
#endif

/*
 * IMPORTANT - If you add/change any of these enums make sure that you don't break
 *			 the compact memory model implementations. See struct GLSLFullySpecifiedType.
 *
 * ALSO NOTE - The semantic code relies on all the types being specified in this order.
 *
 */
typedef enum GLSLTypeSpecifierTAG	
{	
	GLSLTS_INVALID,
	GLSLTS_VOID,
	GLSLTS_FLOAT,
	GLSLTS_VEC2,
	GLSLTS_VEC3,
	GLSLTS_VEC4,
	GLSLTS_INT,
	GLSLTS_IVEC2,
	GLSLTS_IVEC3,
	GLSLTS_IVEC4,
	GLSLTS_BOOL,
	GLSLTS_BVEC2,
	GLSLTS_BVEC3,
	GLSLTS_BVEC4,
	GLSLTS_MAT2X2,
	GLSLTS_MAT2X3,
	GLSLTS_MAT2X4,
	GLSLTS_MAT3X2,
	GLSLTS_MAT3X3,
	GLSLTS_MAT3X4,
	GLSLTS_MAT4X2,
	GLSLTS_MAT4X3,
	GLSLTS_MAT4X4,
	GLSLTS_SAMPLER1D,
	GLSLTS_SAMPLER2D,
	GLSLTS_SAMPLER3D,
	GLSLTS_SAMPLERCUBE,
	GLSLTS_SAMPLER1DSHADOW,			/* These shadow samplers must be together */
	GLSLTS_SAMPLER2DSHADOW,
#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
	GLSLTS_SAMPLER2DRECT,
	GLSLTS_SAMPLER2DRECTSHADOW,
#endif
	GLSLTS_SAMPLERSTREAM,
	GLSLTS_SAMPLEREXTERNAL,
	GLSLTS_STRUCT,
	GLSLTS_NUM_TYPES,

	GLSLTS_FORCE32 = 0x7FFFFFFF

}GLSLTypeSpecifier;

typedef struct GLSLTypeSpecifierInfoTAG
{
#ifdef DEBUG 
	GLSLTypeSpecifier	eTypeSpecifier;
	#define				GLSLTS_(type) GLSLTS_##type,
#else
	#define				GLSLTS_(type)
#endif
	IMG_UINT8			uElements;
	IMG_UINT8			uDimension;
	IMG_UINT8			uSize;
	IMG_UINT8			uIndexed;
	IMG_UINT8			eBaseType;
	IMG_CHAR			*pszFullDesc;
	IMG_CHAR			*pszDesc;
	IMG_UINT8			uColumnsCM;
	IMG_UINT8			uRowsCM;
	IMG_UINT8			bIsNumber;
} GLSLTypeSpecifierInfo;

/* Include glsltabs.c to get this table */
extern const GLSLTypeSpecifierInfo asGLSLTypeSpecifierInfoTable[GLSLTS_NUM_TYPES];

#ifdef DEBUG
#define GLSLTypeSpecifierEnumTable(type)			asGLSLTypeSpecifierInfoTable[type].eTypeSpecifier
#endif
#define GLSLTypeSpecifierNumElementsTable(type)		asGLSLTypeSpecifierInfoTable[type].uElements
#define GLSLTypeSpecifierSizeTable(type)			asGLSLTypeSpecifierInfoTable[type].uSize
#define GLSLTypeSpecifierDimensionTable(type)		asGLSLTypeSpecifierInfoTable[type].uDimension
#define GLSLTypeSpecifierIndexedTable(type)			asGLSLTypeSpecifierInfoTable[type].uIndexed
#define GLSLTypeSpecifierFullDescTable(type)		asGLSLTypeSpecifierInfoTable[type].pszFullDesc
#define GLSLTypeSpecifierDescTable(type)			asGLSLTypeSpecifierInfoTable[type].pszDesc
#define GLSLTypeSpecifierBaseTypeTable(type)		asGLSLTypeSpecifierInfoTable[type].eBaseType
#define GLSLTypeSpecifierNumColumnsCMTable(type)	asGLSLTypeSpecifierInfoTable[type].uColumnsCM
#define GLSLTypeSpecifierNumRowsCMTable(type)		asGLSLTypeSpecifierInfoTable[type].uRowsCM
#define GLSLTypeSpecifierIsNumberTable(type)		asGLSLTypeSpecifierInfoTable[type].bIsNumber

/* Some helpful macros */
#define GLSL_IS_FLOAT(a)   (a >= GLSLTS_FLOAT	 && a <= GLSLTS_VEC4)
	#define GLSL_IS_INT(a)				(a >= GLSLTS_INT					&& a <= GLSLTS_IVEC4)
	#define GLSL_IS_INT_SCALAR(a)		(a == GLSLTS_INT)
	#define GLSL_IS_DEPTH_TEXTURE(a)	(a == GLSLTS_SAMPLER1DSHADOW		|| a == GLSLTS_SAMPLER2DSHADOW)

#define GLSL_IS_SAMPLER(a)				(a >= GLSLTS_SAMPLER1D	&& a <= GLSLTS_SAMPLEREXTERNAL)
#define GLSL_IS_BOOL(a)					(a >= GLSLTS_BOOL	  && a <= GLSLTS_BVEC4)
#define GLSL_IS_MATRIX(a)				(a >= GLSLTS_MAT2X2	&& a <= GLSLTS_MAT4X4)
#define GLSL_IS_STRUCT(a)				(a == GLSLTS_STRUCT)
#define GLSL_IS_NUMBER(a)				(GLSLTypeSpecifierIsNumberTable(a))
#define GLSL_IS_MAT_2ROWS(a)			(a == GLSLTS_MAT2X2 || a == GLSLTS_MAT3X2 || a == GLSLTS_MAT4X2)

	#define GLSL_IS_SCALAR(a)			(a == GLSLTS_BOOL || a == GLSLTS_INT || a == GLSLTS_FLOAT)
	#define GLSL_IS_VECTOR(a)		\
		(a == GLSLTS_VEC2  ||		\
		 a == GLSLTS_VEC3  ||		\
		 a == GLSLTS_VEC4  ||		\
		 a == GLSLTS_IVEC2 ||		\
		 a == GLSLTS_IVEC3 ||		\
		 a == GLSLTS_IVEC4 ||		\
		 a == GLSLTS_BVEC2 ||		\
		 a == GLSLTS_BVEC3 ||		\
		 a == GLSLTS_BVEC4 )		

	#define GLSL_IS_VEC2(a) (a == GLSLTS_VEC2 || a == GLSLTS_IVEC2 || a == GLSLTS_BVEC2)


/*
 * IMPORTANT - If you add/change any of these enums make sure that you don't break
 *			 the compact memory model implementations. See struct GLSLFullySpecifiedType.
 */
typedef enum GLSLTypeQualifierTAG
{
	GLSLTQ_INVALID								= 0x00000000,
	GLSLTQ_TEMP									= 0x00000001,
	GLSLTQ_CONST								= 0x00000002,
	GLSLTQ_UNIFORM								= 0x00000003,
	GLSLTQ_VERTEX_IN							= 0x00000004, /* attribute */
	GLSLTQ_VERTEX_OUT							= 0x00000005, /* varying for vertex shader */
	GLSLTQ_FRAGMENT_IN							= 0x00000006, /* varying for fragment shader */
	GLSLTQ_FRAGMENT_OUT							= 0x00000007, /* gl_FragColor, gl_FragData, or custom fragment output variable. */
	GLSLTQ_NUM
} GLSLTypeQualifier;

extern const IMG_CHAR* const apszGLSLTypeQualifierFullDescTable[GLSLTQ_NUM];
#define GLSLTypeQualifierFullDescTable(TypeQualifier) apszGLSLTypeQualifierFullDescTable[TypeQualifier]


/*
 * IMPORTANT - If you add/change any of these enums make sure that you don't break
 *			 the compact memory model implementations. See struct GLSLFullySpecifiedType.
 */
typedef enum GLSLVaryingModifierFlagsTAG
{
	GLSLVMOD_NONE								= 0x00000000,
	GLSLVMOD_INVARIANT							= 0x00000001,
	GLSLVMOD_CENTROID							= 0x00000002,
	GLSLVMOD_ALL								= 0x0000001F,
	GLSLVMOD_INVALID							= 0x00000020,

} GLSLVaryingModifierFlags;


/*
 * IMPORTANT - If you add/change any of these enums make sure that you don't break
 *			 the compact memory model implementations. See struct GLSLFullySpecifiedType 
 *			 and the macro PREC_VAL.
 */
typedef enum GLSLPrecisionQualifierTAG
{
	GLSLPRECQ_UNKNOWN							= 0x00000000,
	GLSLPRECQ_LOW								= 0x00000001,
	GLSLPRECQ_MEDIUM							= 0x00000002,
	GLSLPRECQ_HIGH								= 0x00000003,
	GLSLPRECQ_INVALID							= 0x00000004,

	GLSLPRECQ_FORCE32							= 0x7FFFFFFF

} GLSLPrecisionQualifier;

extern const IMG_CHAR* const GLSLPrecisionQualifierFullDescTable[GLSLPRECQ_HIGH + 1];

typedef enum GLSLBuiltInVariableIDTAG
{
	/* Not a builtin variable */
	GLSLBV_NOT_BTIN								= 0x00000000,

	/* Vertex shader builtin special */
	/*
		vec4 gl_Position; // must be written to
		float gl_PointSize; // may be written to
		vec4 gl_ClipVertex; // may be written to
	*/
	GLSLBV_POSITION								= 0x00000001,
	GLSLBV_POINTSIZE							= 0x00000002,
	GLSLBV_CLIPVERTEX							= 0x00000003,

	/* fragment shader builtin special variable */
	/*
		vec4 gl_FragCoord;
		bool gl_FrontFacing;
		vec4 gl_FragColor;
		vec4 gl_FragData[gl_MaxDrawBuffers];
		float gl_FragDepth;
	*/
	GLSLBV_FRAGCOORD							= 0x00000004,
	GLSLBV_FRONTFACING							= 0x00000005,
	GLSLBV_FRAGCOLOR							= 0x00000006,
	GLSLBV_FRAGDATA								= 0x00000007,
	GLSLBV_FRAGDEPTH							= 0x00000008,

	/* Builtin attributes */
	/*
		//
		// Vertex Attributes, p. 19.
		//
		attribute vec4 gl_Color;
		attribute vec4 gl_SecondaryColor;
		attribute vec3 gl_Normal;
		attribute vec4 gl_Vertex;
		attribute vec4 gl_MultiTexCoord0;
		attribute vec4 gl_MultiTexCoord1;
		attribute vec4 gl_MultiTexCoord2;
		attribute vec4 gl_MultiTexCoord3;
		attribute vec4 gl_MultiTexCoord4;
		attribute vec4 gl_MultiTexCoord5;
		attribute vec4 gl_MultiTexCoord6;
		attribute vec4 gl_MultiTexCoord7;
		attribute float gl_FogCoord;
	*/
	GLSLBV_COLOR								= 0x00000009,
	GLSLBV_SECONDARYCOLOR						= 0x0000000A,
	GLSLBV_NORMAL								= 0x0000000B,
	GLSLBV_VERTEX								= 0x0000000C,
	GLSLBV_MULTITEXCOORD0						= 0x0000000D,
	GLSLBV_MULTITEXCOORD1						= 0x0000000E,
	GLSLBV_MULTITEXCOORD2						= 0x0000000F,
	GLSLBV_MULTITEXCOORD3						= 0x00000010,
	GLSLBV_MULTITEXCOORD4						= 0x00000011,
	GLSLBV_MULTITEXCOORD5						= 0x00000012,
	GLSLBV_MULTITEXCOORD6						= 0x00000013,
	GLSLBV_MULTITEXCOORD7						= 0x00000014,
	GLSLBV_FOGCOORD								= 0x00000015,

	/* Builtin uniforms */

	/* Built-In Uniform State

		//
		// Matrix state. p. 31, 32, 37, 39, 40.
		//
		uniform mat4 gl_ModelViewMatrix;
		uniform mat4 gl_ProjectionMatrix;
		uniform mat4 gl_ModelViewProjectionMatrix;
		uniform mat4 gl_TextureMatrix[gl_MaxTextureCoords];
	*/

	GLSLBV_MODELVIEWMATRIX						= 0x00000016,
	GLSLBV_PROJECTMATRIX						= 0x00000017,
	GLSLBV_MODELVIEWPROJECTIONMATRIX			= 0x00000018,
	GLSLBV_TEXTUREMATRIX						= 0x00000019,

	/*
		//
		// Derived matrix state that provides inverse and transposed versions
		// of the matrices above. Poorly conditioned matrices may result
		// in unpredictable values in their inverse forms.
		//
		uniform mat3 gl_NormalMatrix; // transpose of the inverse of the
		// upper leftmost 3x3 of gl_ModelViewMatrix
		uniform mat4 gl_ModelViewMatrixInverse;
		uniform mat4 gl_ProjectionMatrixInverse;
		uniform mat4 gl_ModelViewProjectionMatrixInverse;
		uniform mat4 gl_TextureMatrixInverse[gl_MaxTextureCoords];
		uniform mat4 gl_ModelViewMatrixTranspose;
		uniform mat4 gl_ProjectionMatrixTranspose;
		uniform mat4 gl_ModelViewProjectionMatrixTranspose;
		uniform mat4 gl_TextureMatrixTranspose[gl_MaxTextureCoords];
		uniform mat4 gl_ModelViewMatrixInverseTranspose;
		uniform mat4 gl_ProjectionMatrixInverseTranspose;
		uniform mat4 gl_ModelViewProjectionMatrixInverseTranspose;
		uniform mat4 gl_TextureMatrixInverseTranspose[gl_MaxTextureCoords];
*/
	GLSLBV_NORMALMATRIX							= 0x0000001A,
	GLSLBV_MODELVIEWMATRIXINVERSE				= 0x0000001B,
	GLSLBV_PROJECTMATRIXINVERSE					= 0x0000001C,
	GLSLBV_MODELVIEWPROJECTIONMATRIXINVERSE		= 0x0000001D,
	GLSLBV_TEXTUREMATRIXINVERSE					= 0x0000001E,
	GLSLBV_MODELVIEWMATRIXTRANSPOSE				= 0x0000001F,
	GLSLBV_PROJECTIONMATRIXTRANSPOSE			= 0x00000020,
	GLSLBV_MODELVIEWPROJECTIONMATRIXTRANSPOSE	= 0x00000021,
	GLSLBV_TEXTUREMATRIXTRANSPOSE				= 0x00000022,
	GLSLBV_MODELVIEWMATRIXINVERSETRANSPOSE		= 0x00000023,
	GLSLBV_PROJECTMATRIXINVERSETRANSPOSE		= 0x00000024,
	GLSLBV_MODELVIEWPROJECTIONMATRIXINVERSETRANSPOSE = 0x00000025,
	GLSLBV_TEXTUREMATRIXINVERSETRANSPOSE		= 0x00000026,


	/*
		//
		// Normal scaling p. 39.
		//
		uniform float gl_NormalScale;
	*/
	GLSLBV_NORMALSCALE							= 0x00000027,

	/*
		//
		// Depth range in window coordinates, p. 33
		//
		struct gl_DepthRangeParameters {
			float near; // n
			float far; // f
			float diff; // f - n
		};
		uniform gl_DepthRangeParameters gl_DepthRange;
	*/
	GLSLBV_DEPTHRANGE							= 0x00000028,

	/*
		//
		// Clip planes p. 42.
		//
		uniform vec4 gl_ClipPlane[gl_MaxClipPlanes];
	*/
	GLSLBV_CLIPPLANE							 = 0x00000029,

	/*
		//
		// Point Size, p. 66, 67.
		//
		struct gl_PointParameters {
		float size;
		float sizeMin;
		float sizeMax;
		float fadeThresholdSize;
		float distanceConstantAttenuation;
		float distanceLinearAttenuation;
		float distanceQuadraticAttenuation;
		};
		uniform gl_PointParameters gl_Point;
	*/
	GLSLBV_POINT								= 0x0000002A,

	/*
		//
		// Material State p. 50, 55.
		//
		struct gl_MaterialParameters {
		vec4 emission; // Ecm
		vec4 ambient; // Acm
		vec4 diffuse; // Dcm
		vec4 specular; // Scm
		float shininess; // Srm
		};
		uniform gl_MaterialParameters gl_FrontMaterial;
		uniform gl_MaterialParameters gl_BackMaterial;
	*/
	GLSLBV_FRONTMATERIAL						= 0x0000002B,
	GLSLBV_BACKMATERIAL							= 0x0000002C,

	/*

		//
		// Light State p 50, 53, 55.
		//
		struct gl_LightSourceParameters {
			vec4 ambient; // Acli
			vec4 diffuse; // Dcli
			vec4 specular; // Scli
			vec4 position; // Ppli
			vec4 halfVector; // Derived: Hi
			vec3 spotDirection; // Sdli
			float spotExponent; // Srli
			float spotCutoff; // Crli
			// (range: [0.0,90.0], 180.0)
			float spotCosCutoff; // Derived: cos(Crli)
			// (range: [1.0,0.0],-1.0)
			float constantAttenuation; // K0
			float linearAttenuation; // K1
			float quadraticAttenuation;// K2
		};
		uniform gl_LightSourceParameters gl_LightSource[gl_MaxLights];
	*/
	GLSLBV_LIGHTSOURCE							= 0x0000002D,
	
	/*
		struct gl_LightModelParameters {
			vec4 ambient; // Acs
		};
		uniform gl_LightModelParameters gl_LightModel;
	*/
	GLSLBV_LIGHTMODEL							= 0x0000002E,

	/*
		//
		// Derived state from products of light and material.
		//
		struct gl_LightModelProducts {
			vec4 sceneColor; // Derived. Ecm + Acm * Acs
		};
		uniform gl_LightModelProducts gl_FrontLightModelProduct;
		uniform gl_LightModelProducts gl_BackLightModelProduct;
	*/
	GLSLBV_FRONTLIGHTMODELPRODUCT				= 0x0000002F,
	GLSLBV_BACKLIGHTMODELPRODUCT				= 0x00000030,

	/*
		struct gl_LightProducts {
			vec4 ambient; // Acm * Acli
			vec4 diffuse; // Dcm * Dcli
			vec4 specular; // Scm * Scli
		};
		uniform gl_LightProducts gl_FrontLightProduct[gl_MaxLights];
		uniform gl_LightProducts gl_BackLightProduct[gl_MaxLights];
	*/
	GLSLBV_FRONTLIGHTPRODUCT					= 0x00000031,
	GLSLBV_BACKLIGHTPRODUCT						= 0x00000032,

	/*

		//
		// Texture Environment and Generation, p. 152, p. 40-42.
		//
		uniform vec4 gl_TextureEnvColor[gl_MaxTextureImageUnits];
		uniform vec4 gl_EyePlaneS[gl_MaxTextureCoords];
		uniform vec4 gl_EyePlaneT[gl_MaxTextureCoords];
		uniform vec4 gl_EyePlaneR[gl_MaxTextureCoords];
		uniform vec4 gl_EyePlaneQ[gl_MaxTextureCoords];
		uniform vec4 gl_ObjectPlaneS[gl_MaxTextureCoords];
		uniform vec4 gl_ObjectPlaneT[gl_MaxTextureCoords];
		uniform vec4 gl_ObjectPlaneR[gl_MaxTextureCoords];
		uniform vec4 gl_ObjectPlaneQ[gl_MaxTextureCoords];
	*/
	GLSLBV_TEXTUREENVCOLOR						= 0x00000033,
	GLSLBV_EYEPLANES							= 0x00000034,
	GLSLBV_EYEPLANET							= 0x00000035,
	GLSLBV_EYEPLANER							= 0x00000036,
	GLSLBV_EYEPLANEQ							= 0x00000037,
	GLSLBV_OBJECTPLANES							= 0x00000038,
	GLSLBV_OBJECTPLANET							= 0x00000039,
	GLSLBV_OBJECTPLANER							= 0x0000003A,
	GLSLBV_OBJECTPLANEQ							= 0x0000003B,

	/*
		//
		// Fog p. 161
		//
		struct gl_FogParameters {
			vec4 color;
			float density;
			float start;
			float end;
			float scale; // Derived: 1.0 / (end - start)
		};
		uniform gl_FogParameters gl_Fog;
	*/
	GLSLBV_FOG									= 0x0000003C,

	/* vertex shader varying variables 

		varying vec4 gl_FrontColor;
		varying vec4 gl_BackColor;
		varying vec4 gl_FrontSecondaryColor;
		varying vec4 gl_BackSecondaryColor;
		varying vec4 gl_TexCoord[]; // at most will be gl_MaxTextureCoords
		varying float gl_FogFragCoord;
	*/
	GLSLBV_FRONTCOLOR							= 0x0000003D,
	GLSLBV_BACKCOLOR							= 0x0000003E,
	GLSLBV_FRONTSECONDARYCOLOR					= 0x0000003F,
	GLSLBV_BACKSECONDARYCOLOR					= 0x00000040,
	GLSLBV_TEXCOORD								= 0x00000041,
	GLSLBV_FOGFRAGCOORD							= 0x00000042,

	/*  fragment shader varying variables

		varying vec4 gl_Color;
		varying vec4 gl_SecondaryColor;
		varying vec4 gl_TexCoord[]; // at most will be gl_MaxTextureCoords
		varying float gl_FogFragCoord;

		FOR GLES:
		varying mediump vec2 gl_PointCoord;
	*/
	/*
	  IDs have been defined
	GLSLBV_COLOR,
	GLSLBV_SECONDARYCOLOR,
	GLSLBV_TEXCOORD,
	GLSLBV_FOGFRAGCOORD,
	*/
	
	GLSLBV_POINTCOORD							= 0x00000043,
	GLSLESBV_POINTCOORD							= 0x00000043,


	/*======================================
	//	Below are POWERVR HW specific built-in variables.
	=======================================*/

	/* Built-in variables for two side color selection

		uniform bool gl_PMXTwoSideEnable;
		uniform bool gl_PMXSwapFrontFace;

		For pixel shader, it uses the name and ID of gl_FrontColor, gl_BackColor, gl_FrontSecondaryColor, 
		gl_BackSecondaryColor to determine gl_Color and gl_SecondaryColor:

		gl_FrontFacing = miscFaceType;
		if(gl_SwapFrontFace) gl_FrontFacing = !gl_FrontFacing;

		if(gl_FrontFacing) 
		{
			gl_Color = gl_FrontColor;
			gl_SecondaryColor = gl_FrontSecondaryColor;
		}
		else 
		{
			gl_Color = gl_BackColor;
			gl_SecondaryColor = gl_BackSecondaryColor;
		}

		gl_PMXSwapFrontFace should be true for defaut settings, ie CCW front face and Y inverted is true 
		
	*/
	GLSLBV_PMXTWOSIDEENABLE						= 0x00000044,
	GLSLBV_PMXSWAPFRONTFACE						= 0x00000045,


	/* Built-in variable for point size enable

		uniform bool	gl_PMXPointSizeEnable;
		uniform float	gl_PMXPointSize;
	*/
	GLSLBV_PMXPOINTSIZEENABLE					= 0x00000046,
	GLSLBV_PMXPOINTSIZE							= 0x00000047,
	
	/*
		uniform vec4 gl_PMXDepthTextureDesc0[3];
		uniform vec4 gl_PMXDepthTextureDesc1[3];
		uniform vec4 gl_PMXDepthTextureDesc2[3];
		uniform vec4 gl_PMXDepthTextureDesc3[3];
		uniform vec4 gl_PMXDepthTextureDesc4[3];
		uniform vec4 gl_PMXDepthTextureDesc5[3];
		uniform vec4 gl_PMXDepthTextureDesc6[3];
		uniform vec4 gl_PMXDepthTextureDesc7[3];

		This array of uniforms describes the the depth texture 

		depthTextureDesc[0] - This uniform is used to describe the current depth 
		texture comparison mode. This table describes the values that need to 
		be set to acheive the  required test
		
			  TEXTURE_COMPARE_MODE  TEXTURE_COMPARE_FUNC  depthTextureDesc[0]
		COORD												X  Y  Z  W

			  COMPARE_R_TO_TEXTURE  LESS					1  0  0  1
			  COMPARE_R_TO_TEXTURE  LEQUAL				  1  0  1  1
			  COMPARE_R_TO_TEXTURE  GREATER				 0  1  0  1
			  COMPARE_R_TO_TEXTURE  GEQUAL				  0  1  1  1
			  COMPARE_R_TO_TEXTURE  EQUAL					0  0  1  1
			  COMPARE_R_TO_TEXTURE  NOTEQUAL				1  1  0  1
			  COMPARE_R_TO_TEXTURE  ALWAYS				  1  1  1  1
			  COMPARE_R_TO_TEXTURE  NEVER					0  0  0  1
			  NONE				  LESS					0  0  0  0
			  NONE				  LEQUAL				  0  0  0  0
			  NONE				  GREATER				 0  0  0  0
			  NONE				  GEQUAL				  0  0  0  0
			  NONE				  EQUAL					0  0  0  0
			  NONE				  NOTEQUAL				0  0  0  0
			  NONE				  ALWAYS				  0  0  0  0
			  NONE				  NEVER					0  0  0  0


		depthTextureDesc[1..2]- These uniforms are used to describe the texture 
		format being sampled and are used to produced the correct output

			TEXTURE_DEPTH_MODE  depthTextureDesc[1]  depthTextureDesc[2]  depthTextureDesc[3]
	  CORD						 X  Y  Z  W		 X  Y  Z  W			X  Y  Z  W
			
			GL_INTENSITY			1  1  1  1		 0  0  0  0			1  0  0  0
			GL_ALPHA				0  0  0  1		 0  0  0  0			0  0  0  1
			GL_LUMINANCE			1  1  1  0		 0  0  0  1			1  0  0  0
	

	*/
	GLSLBV_PMXDEPTHTEXTUREDESC0				  = 0x00000048,
	GLSLBV_PMXDEPTHTEXTUREDESC1				  = 0x00000049,
	GLSLBV_PMXDEPTHTEXTUREDESC2				  = 0x0000004A,
	GLSLBV_PMXDEPTHTEXTUREDESC3				  = 0x0000004B,
	GLSLBV_PMXDEPTHTEXTUREDESC4				  = 0x0000004C,
	GLSLBV_PMXDEPTHTEXTUREDESC5				  = 0x0000004D,
	GLSLBV_PMXDEPTHTEXTUREDESC6				  = 0x0000004E,
	GLSLBV_PMXDEPTHTEXTUREDESC7				  = 0x0000004F,

	/*
		These two samplers are used for builtin noise functions.

		uniform sampler2D gl_PMXPermTexture;
		uniform sampler2D gl_PMXGradTexture;
	*/
	GLSLBV_PMXPERMTEXTURE						= 0x00000050,
	GLSLBV_PMXGRADTEXTURE						= 0x00000051,

	/*
	  Min and max values to clamp writes to glColors to.

	  uniform float gl_PMXColourClampMin
	  uniform float gl_PMXColourClampMax
	*/
	GLSLBV_PMXCOLOURCLAMPMIN					 = 0x00000052,
	GLSLBV_PMXCOLOURCLAMPMAX					 = 0x00000053,

	/*
		Fog mode .

		uniform vec4 gl_PMXFogMode = {PassThrough, LINEAR, EXP, EXP2}; //only one of four modes is enabled and set to 1.0, rest of them are set to 0.0

		uniform vec4 gl_PMXFogParam = 
		{
			density * ONE_OVER_LN_TWO,
			density * ONE_OVER_SQRT_LN_TWO,
			-1.0/(end - start),
			end/(end - satrt)
		}		
	*/
	GLSLBV_PMXFOGMODE							= 0x00000054,
	GLSLBV_PMXFOGPARAM							= 0x00000055,

	/*
		Clip planes (in a packed format)

		uniform vec4 gl_PMXClipPlane[gl_MaxClipPlanes];
	*/
	GLSLBV_PMXCLIPPLANE						  = 0x00000056,

	/* Position adjust for gl_FragCoord 
		
		uniform vec4 gl_PMXPosAdjust = {X-size, Y-size - 1, X-clip, Y-clip}; 

		if Y Inverted is true, Y-size = Winheight - 1;
		else y-size = 0
	*/
	GLSLBV_PMXPOSADJUST						  = 0x00000057,

	/* Invert adjust for dFdY 
		
		uniform float gl_PMXInvertdFdY = {invert}; 

		if Y Inverted is true, invert = -1;
		else invert = 1
	*/

	GLSLBV_PMXINVERTDFDY						 = 0x00000058,

	GLSLBV_NUM_BUILTINS						  = 0x00000059,

	/* Not required by GLSL by used when we create a GLSL program to do other stuff (like FF TNL) */
	GLSLBV_SPECIAL_VERTEXBLENDWEIGHT				= 0x0000005A,
	GLSLBV_SPECIAL_VERTEXMATRIXINDEX				= 0x0000005B,
	GLSLBV_SPECIAL_MODELVIEWMATRIXTRANSPOSEPALETTE  = 0x0000005C,
	GLSLBV_SPECIAL_MODELVIEWMATRIXINVERSEPALETTE	= 0x0000005D,

	/*

		//
		// Light State p 50, 53, 55.
		//
		struct gl_LightSourceParameters {
			vec4 ambient; // Acli
			vec4 diffuse; // Dcli
			vec4 specular; // Scli
			vec4 position; // Ppli
			vec4 positionNormalised; // Ppli-Hat
			vec4 halfVector; // Derived: Hi
			vec3 spotDirection; // Sdli
			float spotExponent; // Srli
			float spotCutoff; // Crli
			// (range: [0.0,90.0], 180.0)
			float spotCosCutoff; // Derived: cos(Crli)
			// (range: [1.0,0.0],-1.0)
			float constantAttenuation; // K0
			float linearAttenuation; // K1
			float quadraticAttenuation;// K2
		};
		uniform gl_LightSourceParameters gl_LightSource[gl_MaxLights];
	*/
	GLSLBV_PMXLIGHTSOURCE0						 = 0x0000005E,
	GLSLBV_PMXLIGHTSOURCE1						 = 0x0000005F,
	GLSLBV_PMXLIGHTSOURCE2						 = 0x00000060,
	GLSLBV_PMXLIGHTSOURCE3						 = 0x00000061,
	GLSLBV_PMXLIGHTSOURCE4						 = 0x00000062,
	GLSLBV_PMXLIGHTSOURCE5						 = 0x00000063,
	GLSLBV_PMXLIGHTSOURCE6						 = 0x00000064,
	GLSLBV_PMXLIGHTSOURCE7						 = 0x00000065,

	/* 
		struct gl_LineParameters
		{
			float lineWidth;
			float sizeMin;
			float sizeMax;
			float a;
			float b;
			flaot c;
		}
		gl_LineParameters gl_PMXLineParams;
	*/

	GLSLBV_PMXLINEPARAMS						 = 0x00000066,
	
	GLSLBV_MATRIXPALETTEINDEXCLAMP				 = 0x00000067,


	GLSLBV_NUM_BUILTINS_WITH_SPECIALS			/*= 0x00000068*/,

	GLSLBV_FORCE_I32							= 0x7FFFFFFF

} GLSLBuiltInVariableID;

typedef enum GLSLBuiltInVariableWrittenToTAG
{
	GLSLBVWT_POSITION							= 0x00000001,
	GLSLBVWT_POINTSIZE							= 0x00000002,
	GLSLBVWT_CLIPVERTEX							= 0x00000004,
	GLSLBVWT_FRONTCOLOR							= 0x00000008,
	GLSLBVWT_BACKCOLOR							= 0x00000010,
	GLSLBVWT_FRONTSECONDARYCOLOR				= 0x00000020,
	GLSLBVWT_BACKSECONDARYCOLOR					= 0x00000040,
	GLSLBVWT_TEXCOORD							= 0x00000080,
	GLSLBVWT_FRAGCOLOR							= 0x00000100,
	GLSLBVWT_FRAGDATA							= 0x00000200,
	GLSLBVWT_FRAGDEPTH							= 0x00000400,
	GLSLBVWT_FOGFRAGCOORD						= 0x00000800,
	GLSLBVWT_FORCE_I32							= 0x7FFFFFFF
} GLSLBuiltInVariableWrittenTo;


typedef enum GLSLVaryingRegisterTAG
{
	GLSLVR_POSITION = 0,
	GLSLVR_COLOR0,
	GLSLVR_COLOR1,
	GLSLVR_TEXCOORD0,
	GLSLVR_TEXCOORD1,
	GLSLVR_TEXCOORD2,
	GLSLVR_TEXCOORD3,
	GLSLVR_TEXCOORD4,
	GLSLVR_TEXCOORD5,
	GLSLVR_TEXCOORD6,
	GLSLVR_TEXCOORD7,
	GLSLVR_TEXCOORD8,
	GLSLVR_TEXCOORD9,
	GLSLVR_PTSIZE,
	GLSLVR_FOG,		
	GLSLVR_CLIPPLANE0,
	GLSLVR_CLIPPLANE1,
	GLSLVR_CLIPPLANE2,
	GLSLVR_CLIPPLANE3,
	GLSLVR_CLIPPLANE4,
	GLSLVR_CLIPPLANE5,

	GLSLVR_NUM

}GLSLVaryingRegister;

typedef IMG_UINT32 GLSLVaryingMask;
#define MASK_ONE 1

#define	GLSLVM_POSITION			(MASK_ONE << GLSLVR_POSITION)
#define	GLSLVM_COLOR0			(MASK_ONE << GLSLVR_COLOR0)
#define	GLSLVM_COLOR1			(MASK_ONE << GLSLVR_COLOR1)
#define	GLSLVM_TEXCOORD0		(MASK_ONE << GLSLVR_TEXCOORD0)
#define	GLSLVM_TEXCOORD1		(MASK_ONE << GLSLVR_TEXCOORD1)
#define	GLSLVM_TEXCOORD2		(MASK_ONE << GLSLVR_TEXCOORD2)
#define	GLSLVM_TEXCOORD3		(MASK_ONE << GLSLVR_TEXCOORD3)
#define	GLSLVM_TEXCOORD4		(MASK_ONE << GLSLVR_TEXCOORD4)
#define	GLSLVM_TEXCOORD5		(MASK_ONE << GLSLVR_TEXCOORD5)
#define	GLSLVM_TEXCOORD6		(MASK_ONE << GLSLVR_TEXCOORD6)
#define	GLSLVM_TEXCOORD7		(MASK_ONE << GLSLVR_TEXCOORD7)
#define	GLSLVM_TEXCOORD8		(MASK_ONE << GLSLVR_TEXCOORD8)
#define	GLSLVM_TEXCOORD9		(MASK_ONE << GLSLVR_TEXCOORD9)
#define	GLSLVM_PTSIZE			(MASK_ONE << GLSLVR_PTSIZE)
#define	GLSLVM_FOG				(MASK_ONE << GLSLVR_FOG)
#define	GLSLVM_CLIPPLANE0		(MASK_ONE << GLSLVR_CLIPPLANE0)
#define	GLSLVM_CLIPPLANE1		(MASK_ONE << GLSLVR_CLIPPLANE1)
#define	GLSLVM_CLIPPLANE2		(MASK_ONE << GLSLVR_CLIPPLANE2)
#define	GLSLVM_CLIPPLANE3		(MASK_ONE << GLSLVR_CLIPPLANE3)
#define	GLSLVM_CLIPPLANE4		(MASK_ONE << GLSLVR_CLIPPLANE4)
#define	GLSLVM_CLIPPLANE5		(MASK_ONE << GLSLVR_CLIPPLANE5)

#define GLSL_NUM_VARYING_TYPES GLSLVR_NUM

typedef enum GLSLHWRegTypeTAG
{
	HWREG_FLOAT									= 0x00000001,
	HWREG_TEX									= 0x00000002,
} GLSLHWRegType;

/* Register allocation info */
typedef struct GLSLRegisterInfoTAG
{
	GLSLHWRegType	eRegType;

	union
	{
		/* 
			First component allocated to hold symbol-data, assumming each register holds 4 components,
			Note: 
			1) Attributes and varyings are currently not packed, ie uBaseComp is always a multiple of 4. 
			2) For varyings, uBaseComp is the component in terms of texture coordinates. 
				Each texture coordinate unit contains 4 tex coordinates.
			3) Uniforms can be packed. 
		*/
		IMG_UINT32	uBaseComp;		/* Assigned to attributes, uniforms and varyings */ 

		IMG_UINT32  uTextureUnit;	/* Texture unit assigned to sampler. */
	} u;


	/*
		The number of (contiguous) components allocated to
		hold the data for the symbol. Note that some of these
		allocated components may be unused, depending upon
		how the data for the data-type is arranged. Use
		ui32CompUseMask to determine which components 
		contain the data, and which are unused.
	*/
	IMG_UINT32	uCompAllocCount;

	/*
		Bitmap where the i-th bit is 1 if the i-th component of the variable is used and is 0 otherwise.
		For example, if the variable declaration is "vec4 color;" and we only use "color.rgb",
		then this bitmap is 0x00000007. Currently mat4 variables are the ones that take up 
		the most space: 16 bits. This variable is unused for structs.
	*/
	IMG_UINT32  ui32CompUseMask;

} GLSLRegisterInfo;

/* 
** Binding symbols from GLSL programs to OGL driver
*/
typedef struct GLSLBindingSymbolTAG
{
	/* Name of the symbol */
	IMG_CHAR			*pszName;

	/* Data type and storage qualifer */
	GLSLTypeSpecifier	eTypeSpecifier;
	GLSLTypeQualifier	eTypeQualifier; /* no temp qualifer */
	GLSLPrecisionQualifier	ePrecisionQualifier; 

	/* Varying modifier flags */
	GLSLVaryingModifierFlags eVaryingModifierFlags;

	/* 
	** Active and declared array sizes
	** iDeclaredArraySize = 0 indicates non-array type, iActiveArraySize must be 1 
	** iDeclaredArraySize != 0 indicates array type, iDeclaredArraySize >= iActiveArraySize
	*/
	IMG_INT32			iActiveArraySize;
	IMG_INT32			iDeclaredArraySize;
	
	/* 
		Low level register info for this symbol 
		sRegisterInfo.ui32CompUseMask is 0 for a struct type.
	*/
	GLSLRegisterInfo	sRegisterInfo;

	/*
		Number of base type members and associated member symbols 

		A struct variable has been broken down to GLSL basic types or arrays of basic types. 
		uNumBaseTypeMembers is the number of basic type members.
		For example: if a shader code is 
		struct AA
		{
			int i[10];
			int j;
		};
		struct BB
		{
			AA a[3];
			float f;
		}
		BB bb;

		then "bb" has the following base type members:
			bb.a[0].i
			bb.a[0].j
			bb.a[1].i
			bb.a[1].j
			bb.a[2].i
			bb.a[2].j
			bb.f
		uNumBaseTypeMembers = 7

		If it is a basic type or array of basic type (not a struct), uNumBaseTypeMembers = 0
	*/
	IMG_UINT32					uNumBaseTypeMembers;
	struct GLSLBindingSymbolTAG	*psBaseTypeMembers;
	
	/* BuiltIn variable ID */
	GLSLBuiltInVariableID eBIVariableID;

} GLSLBindingSymbol;

typedef struct GLSLDepthTextureTAG
{
	GLSLBindingSymbol *psTextureSymbol;
	IMG_INT32		  iOffset;
	GLSLBindingSymbol *psTexDescSymbol;
	
}GLSLDepthTexture;


/* 
** A list of all binding symbols 
*/
typedef struct GLSLBindingSymbolListTAG
{
	IMG_UINT32			uNumBindings;
	IMG_UINT32			uMaxBindingEntries;
	GLSLBindingSymbol	*psBindingSymbolEntries;

	/* The number of components assigned for constant data used within the code */
	IMG_UINT32			uNumCompsUsed;

	/* 
	** A pointer to some host memory holding all default constants data (include uniform variables, 
	** constant variables and literal constants). It totally has uNumCompsUsed floats
	*/
	IMG_FLOAT			*pfConstantData;

	/* 
	** Number of depth textures active in the 
	** program and its associated sampler symbol and desc symbol 
	*/
	IMG_UINT32			uNumDepthTextures;
	GLSLDepthTexture	*psDepthTextures;

} GLSLBindingSymbolList;

typedef struct GLSLInfoLogTAG
{
	IMG_CHAR *pszInfoLogString;
	IMG_UINT32  uInfoLogStringLength;
} GLSLInfoLog;

typedef struct GLSLConstRangeTAG
{
	IMG_UINT32 uStart;
	IMG_UINT32 uCount;
}GLSLConstRange;

typedef enum GLSLProgramFlagsTAG
{
	/* Code type flags all mutually exclusive */
	GLSLPF_UNIFLEX_INPUT						 = 0x00000001,
	GLSLPF_UNIFLEX_OUTPUT						 = 0x00000002,
	GLSLPF_NATIVE_HW_CODE						 = 0x00000003,
	GLSLPF_UNIPATCH_INPUT						= 0x00000004,

	/* Can be set in conjunction with flags above */
	GLSLPF_DISCARD_EXECUTED						 = 0x00000008,	/* Is discard executed, apply to fragment program only */
	GLSLPF_FRAGDEPTH_WRITTENTO					= 0x00000010,	/* Is glFragDepth being written to in the program, apply to fragment program only */

	GLSLPF_OUTPUT_LINEWIDTH						 = 0x00010000,	/* Does the shader output line width - only for FFTNL program */

	GLSLPF_FORCE_I32							 = 0x7FFFFFFF

}GLSLProgramFlags;


typedef enum GLSLCompilerWarningsTAG
{
	GLSLCW_WARN_GRADIENT_CALC_INSIDE_CONDITIONAL = 0x00000001,
	GLSLCW_WARN_USE_OF_UNINITIALISED_DATA		= 0x00000002,
	GLSLCW_WARN_FUNCTION_NOT_CALLED			  = 0x00000004,
	GLSLCW_WARN_RECURSIVE_FUNCTION_CALL		  = 0x00000008,

	GLSLCW_WARN_ALL							  = 0x7FFFFFFF,

} GLSLCompilerWarnings;

typedef enum GLSLSupportedFeaturesTAG
{
	GLSLSF_PRECISION						 = 0x00000001,
	GLSLSF_INVARIANCE						= 0x00000002,
	GLSLSF_INT_TO_FLOAT_CONVERSION			= 0x00000004,
	GLSLSF_F_SUFFIX_ON_FLOATS				= 0x00000008,
	GLSLSF_ARRAY_CONSTRUCTORS				= 0x00000010,
	GLSLSF_CENTROID_VARYINGS				 = 0x00000020,
	GLSLSF_ARRAY_ON_TYPES					= 0x00000040,
	GLSLSF_UNIFORM_INITIALISERS			  = 0x00000080,
	GLSLSF_MATRIX_FROM_MATRIX_CONSTRUCTION   = 0x00000100,
	GLSLSF_ARRAYS_FIRST_CLASS_TYPES		  = 0x00000200,
} GLSLSupportedFeatures;

#define GLSLSF_GLSL_100 0

#define GLSLSF_GLSL_110 0

#define GLSLSF_GLSL_120 (GLSLSF_INVARIANCE						| \
						 GLSLSF_INT_TO_FLOAT_CONVERSION		  | \
						 GLSLSF_F_SUFFIX_ON_FLOATS				| \
						 GLSLSF_ARRAY_CONSTRUCTORS				| \
						 GLSLSF_CENTROID_VARYINGS				| \
						 GLSLSF_ARRAY_ON_TYPES					| \
						 GLSLSF_UNIFORM_INITIALISERS			 | \
						 GLSLSF_MATRIX_FROM_MATRIX_CONSTRUCTION  | \
						 GLSLSF_ARRAYS_FIRST_CLASS_TYPES)

#define GLSLSF_GLSLES_100 (GLSLSF_PRECISION | GLSLSF_INVARIANT)


/*
 * Info required for compling to HW Code 
 */
typedef struct GLSLUniFlexHWCodeInfoTAG
{
	UNIFLEX_PROGRAM_PARAMETERS	*psUFParams;
} GLSLUniFlexHWCodeInfo;


/* 
   Structure supplied with InitCompilerContext to control the precision of the 
   data within a shader. 'GL' or 'ES' indicates whether the value is used for that API.
   The value in brackets (u,l,m,h) after this indicates the value that should be 
   supplied if the default behaviour for that API is required. 
*/
typedef struct GLSLRequestedPrecisionsTAG
{
	/* 
		User defined temps, attribs, varyings and uniforms will get these precision 
		if none is explicitly defined 
	*/
	GLSLPrecisionQualifier eDefaultUserVertFloat;   // GL(h) + ES(h)
	GLSLPrecisionQualifier eDefaultUserVertInt;	 // GL(h) + ES(h)
	GLSLPrecisionQualifier eDefaultUserVertSampler; // GL(h) + ES(h)
	GLSLPrecisionQualifier eDefaultUserFragFloat;   // GL(h) + ES(u)
	GLSLPrecisionQualifier eDefaultUserFragInt;	 // GL(h) + ES(m)
	GLSLPrecisionQualifier eDefaultUserFragSampler; // GL(h) + ES(m)

	/*
		Precision used for all booleans
	*/
	GLSLPrecisionQualifier eVertBooleanPrecision;   // GL(h)
	GLSLPrecisionQualifier eFragBooleanPrecision;	// GL(h)

	/* Precisions for the built in variables */
	GLSLPrecisionQualifier eBIStateFloat;		 	// GL(h)		 (gl_ModelViewMatrix, gl_ProjectionMatrix, gl_ModelViewProjectionMatrix etc.)
	GLSLPrecisionQualifier eBIStateInt;				// GL(h) + ES(h) (gl_MaxLights, gl_MaxClipPlanes, gl_MaxTextureUnits, gl_MaxTextureCoords etc.)
	GLSLPrecisionQualifier eBIVertAttribFloat;   	 // GL(h)		 (gl_Seccondary/Color, gl_Normal, gl_Vertex, gl_MultiTexCoord[x], gl_FogCoord)
	GLSLPrecisionQualifier eBIVaryingFloat;			// GL(h)		 (gl_Front/Back/Secondary/Color, gl_TexCoord, gl_FogFragCoord, gl_ClipVertex)  
	GLSLPrecisionQualifier eBIFragFloat;		  	// GL(h) + ES(m) (gl_FragCoord, gl_FragColor, gl_FragData, gl_FragDepth)

	/* Some special cases */
	GLSLPrecisionQualifier eGLPosition;				// GL(h) + ES(h) (gl_Position)
	GLSLPrecisionQualifier eGLPointSize;		  	// GL(h) + ES(m) (gl_PointSize)
	GLSLPrecisionQualifier eGLPointCoord;		 	// ES(m)		 (gl_PointCoord)
	GLSLPrecisionQualifier eDepthRange;				// GL(h) + ES(h) (gl_DepthRangeParameters.near,far,diff) 

	/* 
		Will force user defined precisions to these values (set to UNKNOWN to disable) 
		Will not affect default precisions, adjust values above to do that
	*/
	GLSLPrecisionQualifier eForceUserVertFloat;   	// GL(u) + ES(u)
	GLSLPrecisionQualifier eForceUserVertInt;	 	// GL(u) + ES(u)
	GLSLPrecisionQualifier eForceUserVertSampler; 	// GL(u) + ES(u)
	GLSLPrecisionQualifier eForceUserFragFloat;   	// GL(u) + ES(u)
	GLSLPrecisionQualifier eForceUserFragInt;	 	// GL(u) + ES(u)
	GLSLPrecisionQualifier eForceUserFragSampler; 	// GL(u) + ES(u)

} GLSLRequestedPrecisions;

#define PREC_VAL(uBits) ((GLSLPrecisionQualifier)(uBits & 0x3))

/* Useful if you want to expose a bitfield apphint to set the precisions */
#define SET_BITFIELD_REQUESTED_PRECISION(psRP, uBitfield) \
	psRP->eDefaultUserVertFloat   = PREC_VAL(uBitfield >>  0); \
	psRP->eDefaultUserVertInt	 = PREC_VAL(uBitfield >>  2); \
	psRP->eDefaultUserFragFloat   = PREC_VAL(uBitfield >>  4); \
	psRP->eDefaultUserFragInt	 = PREC_VAL(uBitfield >>  6); \
	psRP->eBIStateFloat			= PREC_VAL(uBitfield >>  8); \
	psRP->eBIStateInt			 = PREC_VAL(uBitfield >> 10); \
	psRP->eBIVertAttribFloat	  = PREC_VAL(uBitfield >> 12); \
	psRP->eBIVaryingFloat		 = PREC_VAL(uBitfield >> 14); \
	psRP->eBIFragFloat			= PREC_VAL(uBitfield >> 16); \
	psRP->eGLPosition			 = PREC_VAL(uBitfield >> 18); \
	psRP->eGLPointSize			= PREC_VAL(uBitfield >> 20); \
	psRP->eGLPointCoord			= PREC_VAL(uBitfield >> 22); \
	psRP->eForceUserVertFloat	 = PREC_VAL(uBitfield >> 24); \
	psRP->eForceUserVertInt		= PREC_VAL(uBitfield >> 26); \
	psRP->eForceUserFragFloat	 = PREC_VAL(uBitfield >> 28); \
	psRP->eForceUserFragInt		= PREC_VAL(uBitfield >> 30); 



/* 
   Provides limits to the resources the compiler will allocate 
   Values on right are specification minimums for that API
*/
typedef struct GLSLCompilerResourcesTAG
{

	/* Shared for both GL and ES */
	IMG_INT32 iGLMaxVertexAttribs;			 // GL ( 16 - ARB_vertex_shader)			ES (8   - ES 2.0)
	IMG_INT32 iGLMaxTextureImageUnits;		 // GL (  8 - ARB_fragment_shader)		 ES (2   - ES 2.0)
	IMG_INT32 iGLMaxDrawBuffers;				// GL (  1 - proposed ARB_draw_buffers)   ES (1   - ES 2.0)
	IMG_INT32 iGLMaxVertexTextureImageUnits;   // GL (  8 - ARB_vertex_shader)			ES (0   - ES 2.0)
	IMG_INT32 iGLMaxCombinedTextureImageUnits; // GL (  2 - ARB_fragment_shader)		 ES (2   - ES 2.0)

#if defined(GLSL_ES)	/* ES only */
	IMG_INT32 iGLMaxVertexUniformVectors;	  // ES (128 - ES 2.0)
	IMG_INT32 iGLMaxVaryingVectors;			// ES (  8 - ES 2.0)
	IMG_INT32 iGLMaxFragmentUniformVectors;	// ES ( 16 - ES 2.0)
#else
	/* GL Only */
	IMG_INT32 iGLMaxVaryingFloats;			 // GL ( 32 - ARB_vertex_shader)
	IMG_INT32 iGLMaxLights;					// GL (  8 - GL 1.0)
	IMG_INT32 iGLMaxClipPlanes;				// GL (  6 - GL 1.0)
	IMG_INT32 iGLMaxTextureUnits;			  // GL (  8 - GL 1.3)
	IMG_INT32 iGLMaxTextureCoords;			 // GL (  8 - ARB_fragment_program)
	IMG_INT32 iGLMaxVertexUniformComponents;   // GL (512 - ARB_vertex_shader)
	IMG_INT32 iGLMaxFragmentUniformComponents; // GL ( 64 - ARB_fragment_shader)

	IMG_BOOL  bNativeFogSupport;			/* If not, a texture coordinate unit will be used for gl_FogFragCoord */

#endif

} GLSLCompilerResources;
  
typedef struct GLSLInlineFuncRulesTAG
{

	/* Always always inline functions */
	IMG_BOOL 	bInlineAlwaysAlways;

	/* We always inline funciton which has sampler parameters */
	IMG_BOOL	bInlineSamplerParamFunc;		/* Inline with sampler parameters. Should be always true */
	IMG_BOOL	bInlineCalledOnceFunc;			/* If function is only called once */

	IMG_UINT32	uNumICInstrsBodyLessThan;		/* If function has less than uNumICInstrsBodyLessThan instructions */
	IMG_UINT32	uNumParamComponentsGreaterThan;	/* If the total number of parameter components greater than uNumParamComponentsGreaterThan, vec4 counts for 4 components */

} GLSLInlineFuncRules;

typedef struct GLSLUnrollLoopRulesTAG
{
	IMG_BOOL	bEnableUnroll;					/* Enable unroll if number of iterations is less than uMaxNumIterations */
	IMG_BOOL	bUnrollRelativeAddressingOnly;	/* Enable unroll if the loop has dynamic indexing and number of iterations is less than uMaxNumIterations*/

	IMG_UINT32  uMaxNumIterations;				/* The max number of iteration for unrolling, this condition should always applies */
} GLSLUnrollLoopRules;

/* Init struct to 0 to be safe from new changes */
typedef struct GLSLInitCompilerContextTAG
{
	/* Need to be set when calling GLSLInitCompiler() */
	GLSLLogFiles				eLogFiles;
	IMG_BOOL					bSuccessfulInit;
	GLSLRequestedPrecisions	 sRequestedPrecisions;
	GLSLCompilerResources		sCompilerResources;

#if defined(DEBUG)
	/* Enable logging of shader analysis */
	IMG_VOID					(*pfnPrintShaders)(const IMG_CHAR *pszFmt, ...) IMG_FORMAT_PRINTF(1, 2);
#endif

	/* Rules for unrolling and inline */
	GLSLInlineFuncRules			sInlineFuncRules;
	GLSLUnrollLoopRules			sUnrollLoopRules;

	/* Setup by  GLSLInitCompiler() - SHOULD BE SET TO NULL */
	IMG_VOID					*pvCompilerPrivateData;

	
	IMG_BOOL					bEnableUSCMemoryTracking;

} GLSLInitCompilerContext;


/* Init struct to 0 to be safe from new changes */
typedef struct GLSLCompileProgramContextTAG
{
	GLSLInitCompilerContext *psInitCompilerContext;
	IMG_CHAR				**ppszSourceCodeStrings;
	IMG_UINT32				uNumSourceCodeStrings;
	GLSLProgramType		  eProgramType;

	IMG_BOOL				 bCompleteProgram;
	IMG_BOOL				 bDisplayMetrics;
	IMG_BOOL				 bValidateOnly;
	GLSLCompilerWarnings	 eEnabledWarnings;

	#ifdef SRC_DEBUG
	IMG_BOOL				 bDumpSrcCycle;
	#ifdef USER_PERFORMANCE_STATS_ONLY
        IMG_BOOL				 bEstimatePerformance;
	#endif	/* USER_PERFORMANCE_STATS_ONLY */
	#endif /* SRC_DEBUG */

} GLSLCompileProgramContext;

#ifdef GLSL_ES
	/* For GLES, the compiler is provided in a shared object, so we need to export it. */
	#define GLSL_EXPORT IMG_EXPORT
#else
	/* For desktop GL, the compiler is provided in a static library. */
	#define GLSL_EXPORT IMG_INTERNAL
#endif

/* These functions are actually exported by the compiler when built as a dll/so */
IMG_IMPORT IMG_BOOL IMG_CALLCONV GLSLInitCompiler(GLSLInitCompilerContext *psInitCompilerContext);
IMG_IMPORT IMG_BOOL IMG_CALLCONV GLSLShutDownCompiler(GLSLInitCompilerContext *psInitCompilerContext);


/*******************************************************************************************************************************************************/
/*******************************************************************************************************************************************************/

/* These functions are used by various drivers linking to the compiler when built as a lib */
IMG_VOID IMG_CALLCONV GLSLDumpUniFlexProgram(IMG_VOID *pvCode, FILE *fstream);

IMG_VOID IMG_CALLCONV GLSLGenerateInfoLog(GLSLInfoLog *psInfoLog, ErrorLog *psErrorLog, ErrorType eErrorTypes, IMG_BOOL bSuccess);

IMG_VOID IMG_CALLCONV GLSLFreeInfoLog(GLSLInfoLog *psInfoLog);

IMG_BOOL IMG_CALLCONV GLSLFreeBuiltInState(GLSLInitCompilerContext *psInitCompilerContext);

IMG_BOOL IMG_CALLCONV GLSLGetLanguageVersion(IMG_UINT32 uStringLength, IMG_CHAR *pszVersionString);

#endif // __gl_glsl_h_

/******************************************************************************
 End of file (glsl.h)
******************************************************************************/

