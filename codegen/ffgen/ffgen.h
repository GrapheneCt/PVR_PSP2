/******************************************************************************
 * Name         : ffgen.h
 * Author       : James McCarthy
 * Created      : 07/11/2005
 *
 * Copyright    : 2000-2008 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: ffgen.h $
 *****************************************************************************/

#ifndef __gl_ffgen_h_
#define __gl_ffgen_h_

#include "img_types.h"
#if !defined(OGLES1_MODULE) || defined(FFGEN_UNIFLEX)
/* File scope static for g_asInputInstDesc from usc.h must exist as g_asInputInstDesc is used in other source files from the same translation unit. */
/* PRQA S 3207 ++ */
#include "usc.h"
/* PRQA S 3207 -- */
#endif /* !defined(OGLES1_MODULE) || defined(FFGEN_UNIFLEX) */
#include "use.h"
#include "reginfo.h"

#define FFTNLGEN_MAX_NUM_TEXTURE_UNITS (16UL)

#if defined(STANDALONE) || defined(DEBUG)
#define MAX_DESC_SIZE 35
#endif

/* 
	Enables for fftnl generation 
*/
#define FFTNL_ENABLES1_VERTEXBLENDING          0x00000001
#define FFTNL_ENABLES1_TRANSFORMNORMALS        0x00000002
#define FFTNL_ENABLES1_MATRIXPALETTEBLENDING   0x00000004
#define FFTNL_ENABLES1_STANDARDTRANSFORMATION  0x00000008
#define FFTNL_ENABLES1_NORMALISENORMALS        0x00000010
#define FFTNL_ENABLES1_FOGCOORD                0x00000020
#define FFTNL_ENABLES1_FOGLINEAR               0x00000040
#define FFTNL_ENABLES1_FOGEXP                  0x00000080
#define FFTNL_ENABLES1_FOGEXP2                 0x00000100
#define FFTNL_ENABLES1_CLIPPING                0x00000200
#define FFTNL_ENABLES1_EYEPOSITION             0x00000400
#define FFTNL_ENABLES1_VERTEXTOEYEVECTOR       0x00000800
#define FFTNL_ENABLES1_LIGHTINGENABLED         0x00001000
#define FFTNL_ENABLES1_LIGHTINGTWOSIDED        0x00002000
#define FFTNL_ENABLES1_LOCALVIEWER             0x00004000
#define FFTNL_ENABLES1_SEPARATESPECULAR        0x00008000
#define FFTNL_ENABLES1_TEXTURINGENABLED        0x00010000
#define FFTNL_ENABLES1_TEXGENSPHEREMAP         0x00020000
#define FFTNL_ENABLES1_REFLECTIONMAP           0x00040000
#define FFTNL_ENABLES1_COLOURMATERIAL          0x00080000
#define FFTNL_ENABLES1_POINTSIZEARRAY          0x00100000
#define FFTNL_ENABLES1_TEXGEN                  0x00200000
#define FFTNL_ENABLES1_USEMVFORNORMALTRANSFORM 0x00400000
#define FFTNL_ENABLES1_POINTATTEN              0x00800000
#define FFTNL_ENABLES1_UNCLAMPEDCOLOURS        0x01000000
#define FFTNL_ENABLES1_SECONDARYCOLOUR         0x02000000
#define FFTNL_ENABLES1_FOGCOORDEYEPOS          0x04000000
#define FFTNL_ENABLES1_VERTCOLOUREMISSIVE      0x08000000
#define FFTNL_ENABLES1_VERTCOLOURAMBIENT       0x10000000
#define FFTNL_ENABLES1_VERTCOLOURDIFFUSE       0x20000000
#define FFTNL_ENABLES1_VERTCOLOURSPECULAR      0x40000000
#define FFTNL_ENABLES1_RANGEFOG                0x80000000


#define FFTNL_ENABLES2_POINTS		           0x00000001
#define FFTNL_ENABLES2_POINTSPRITES            0x00000002
#define FFTNL_ENABLES2_FOGCOORDZERO            0x00000008

#if defined(FIX_HW_BRN_25211)
#define FFTNL_ENABLES2_CLAMP_OUTPUT_COLOURS	   0x00000010
#endif /* defined(FIX_HW_BRN_25211) */

#define FFTNL_ENABLES2_TEXCOORD_INDICES		   0x00000020


/* 
	Code gen method 
*/
typedef enum FFCodeGenMethod_TAG
{
	FFCGM_ONE_PASS, /* Faster and uses less memory     */
	FFCGM_TWO_PASS, /* Can produce slighty faster code */
} FFCodeGenMethod;

/* 
	Code gen flags 
*/
typedef enum FFCodeGenFlags_TAG
{
	FFGENCGF_REDIRECT_OUTPUT_TO_INPUT	= 0x00000001,	/* Redirect all output register writes to the input
														   registers - used for complex geometry */
	FFGENCGF_INPUT_REG_SIZE_4			= 0x00000002,	/* Input register size should always be a multiple of 4s.
														   OGL 2.0 sets this flag */
} FFCodeGenFlags;


#define ROUND_BYTE_ARRAY_TO_DWORD(a) ((a + 3) & ~0x3UL)


/* 
	Structure which descives the current setup of the fixed function transform and lighting pipeline 
*/
typedef struct FFTNLGenDesc_TAG 
{
	/* Describes the FF pipe set up */
	IMG_UINT32				  ui32FFTNLEnables1;
	IMG_UINT32				  ui32FFTNLEnables2;

	/* Secondary attribute setup */
	IMG_UINT32                uSecAttrConstBaseAddressReg;
	IMG_UINT32                uSecAttrStart;
	IMG_UINT32                uSecAttrEnd;
	IMG_UINT32                uSecAttrAllOther;

	/* Code generation method */
	FFCodeGenMethod			  eCodeGenMethod;

	/* Code generation flags */
	IMG_UINT32				  eCodeGenFlags;

	/* Clip plane enables */
	IMG_UINT32                uEnabledClipPlanes;

	IMG_UINT32                uNumBlendUnits;
	IMG_UINT32                uNumMatrixPaletteEntries;

	/* 
	   Bit mask describing the various enabled light types 
	    - all our mutually exclusive, their should be no shared bits
	*/
	IMG_UINT32                uEnabledSpotLocalLights;
	IMG_UINT32                uEnabledSpotInfiniteLights;
	IMG_UINT32                uEnabledPointLocalLights;
	IMG_UINT32                uEnabledPointInfiniteLights;
	IMG_UINT32                uLightHasSpecular;
	

	/* Colour mask setup */
	//IMG_UINT8                 ubColourMaterialMask[ROUND_BYTE_ARRAY_TO_DWORD(4)];
	//IMG_UINT8                 ubColourMaterialMaskBack[ROUND_BYTE_ARRAY_TO_DWORD(4)];

	/* Which tex coords to pass through */
	IMG_UINT32                uEnabledPassthroughCoords;
	IMG_UINT8                 aubPassthroughCoordMask[ROUND_BYTE_ARRAY_TO_DWORD(FFTNLGEN_MAX_NUM_TEXTURE_UNITS)];
	IMG_UINT8            	  aubPassthroughCoordIndex[ROUND_BYTE_ARRAY_TO_DWORD(FFTNLGEN_MAX_NUM_TEXTURE_UNITS)];
	
	/* Which tex coords should be generated using eye linear */
	IMG_UINT32                uEnabledEyeLinearCoords;
	IMG_UINT8                 aubEyeLinearCoordMask[ROUND_BYTE_ARRAY_TO_DWORD(FFTNLGEN_MAX_NUM_TEXTURE_UNITS)];

	/* Which tex coords should be generated using obj linear */
	IMG_UINT32                uEnabledObjLinearCoords;
	IMG_UINT8                 aubObjLinearCoordMask[ROUND_BYTE_ARRAY_TO_DWORD(FFTNLGEN_MAX_NUM_TEXTURE_UNITS)];
	
	/* Which tex coords should be generated using sphere map */
	IMG_UINT32                uEnabledSphereMapCoords;
	IMG_UINT8                 aubSphereMapCoordMask[ROUND_BYTE_ARRAY_TO_DWORD(FFTNLGEN_MAX_NUM_TEXTURE_UNITS)];

	/* Which tex coords should be generated using normal map */
	IMG_UINT32                uEnabledNormalMapCoords;
	IMG_UINT8                 aubNormalMapCoordMask[ROUND_BYTE_ARRAY_TO_DWORD(FFTNLGEN_MAX_NUM_TEXTURE_UNITS)];

	/* Which tex coords should be generated using position map */
	IMG_UINT32                uEnabledPositionMapCoords;
	IMG_UINT8                 aubPositionMapCoordMask[ROUND_BYTE_ARRAY_TO_DWORD(FFTNLGEN_MAX_NUM_TEXTURE_UNITS)];

	/* Which tex coords should be generated using reflection map */
	IMG_UINT32                uEnabledReflectionMapCoords;
	IMG_UINT8                 aubReflectionMapCoordMask[ROUND_BYTE_ARRAY_TO_DWORD(FFTNLGEN_MAX_NUM_TEXTURE_UNITS)];

	
	/* Which tex coords should be transformed by the texture matrices */
	IMG_UINT32                uEnabledTextureMatrices;

	/* Which vertex blend units are enabled - THINK THIS MIGHT BE POINTLESS */
	IMG_UINT32                uEnabledVertexBlendUnits;
	

} FFTNLGenDesc;


/* 
	Binding register description
*/
typedef enum FFGenRegDescTAG
{
	/*
	  Vertex shader inputs

	  vec4  gl_Color;
	  vec4  gl_SecondaryColor;
	  vec3  gl_Normal;
	  vec4  gl_Vertex;
	  vec4  gl_MultiTexCoord0;
	  vec4  gl_MultiTexCoord1;
	  vec4  gl_MultiTexCoord2;
	  vec4  gl_MultiTexCoord3;
	  vec4  gl_MultiTexCoord4;
	  vec4  gl_MultiTexCoord5;
	  vec4  gl_MultiTexCoord6;
	  vec4  gl_MultiTexCoord7;
	  float gl_FogCoord;
	*/
	FFGEN_INPUT_COLOR,             /* GLSLBV_COLOR	                   */
	FFGEN_INPUT_SECONDARYCOLOR,    /* GLSLBV_SECONDARYCOLOR            */
	FFGEN_INPUT_NORMAL,            /* GLSLBV_NORMAL                    */
	FFGEN_INPUT_VERTEX,            /* GLSLBV_VERTEX                    */
	FFGEN_INPUT_MULTITEXCOORD0,    /* GLSLBV_MULTITEXCOORD0            */
	FFGEN_INPUT_MULTITEXCOORD1,    /* GLSLBV_MULTITEXCOORD1            */
	FFGEN_INPUT_MULTITEXCOORD2,    /* GLSLBV_MULTITEXCOORD2            */
	FFGEN_INPUT_MULTITEXCOORD3,    /* GLSLBV_MULTITEXCOORD3            */
	FFGEN_INPUT_MULTITEXCOORD4,    /* GLSLBV_MULTITEXCOORD4            */
	FFGEN_INPUT_MULTITEXCOORD5,    /* GLSLBV_MULTITEXCOORD5            */
	FFGEN_INPUT_MULTITEXCOORD6,    /* GLSLBV_MULTITEXCOORD6            */
	FFGEN_INPUT_MULTITEXCOORD7,    /* GLSLBV_MULTITEXCOORD7            */
	FFGEN_INPUT_FOGCOORD,          /* GLSLBV_FOGCOORD                  */
	FFGEN_INPUT_VERTEXBLENDWEIGHT, /* GLSLBV_SPECIAL_VERTEXBLENDWEIGHT */
	FFGEN_INPUT_VERTEXMATRIXINDEX, /* GLSLBV_SPECIAL_VERTEXMATRIXINDEX */
	FFGEN_INPUT_POINTSIZE,		   /*  								   */


	/*
	   Vertex shader outputs

		vec4  gl_Position;
		float gl_PointSize;
		vec4  gl_ClipVertex;
		vec4  gl_FrontColor;
		vec4  gl_BackColor;
		vec4  gl_FrontSecondaryColor;
		vec4  gl_BackSecondaryColor;
		vec4  gl_TexCoord[]; // at most will be gl_MaxTextureCoords
		float gl_FogFragCoord;
		vec4  point_sprite; // Place holder;
		float gl_LineWidth; - for car nav exts
		vec2  gl_ViewSpacePos - for car nav exts
	*/
	FFGEN_OUTPUT_POSITION,	          /* GLSLBV_POSITION            */
	FFGEN_OUTPUT_POINTSIZE,   	      /* GLSLBV_POINTSIZE           */
	FFGEN_OUTPUT_CLIPVERTEX,  	      /* GLSLBV_CLIPVERTEX	        */
	FFGEN_OUTPUT_FRONTCOLOR, 	      /* GLSLBV_FRONTCOLOR          */
	FFGEN_OUTPUT_BACKCOLOR, 	      /* GLSLBV_BACKCOLOR           */
	FFGEN_OUTPUT_FRONTSECONDARYCOLOR, /* GLSLBV_FRONTSECONDARYCOLOR */
	FFGEN_OUTPUT_BACKSECONDARYCOLOR,  /* GLSLBV_BACKSECONDARYCOLOR  */
	FFGEN_OUTPUT_TEXCOORD, 	          /* GLSLBV_TEXCOORD            */
	FFGEN_OUTPUT_FOGFRAGCOORD, 	      /* GLSLBV_FOGFRAGCOORD        */
	FFGEN_OUTPUT_POINTSPRITE, 	      /* Place holder               */

	/* 
	   Built-In State

	   mat4 gl_ModelViewMatrix;
	   mat4 gl_ProjectionMatrix;
	   mat4 gl_ModelViewProjectionMatrix;
	   mat4 gl_TextureMatrix[gl_MaxTextureCoords];
	   mat4 gl_ModelViewMatrixInverseTranspose;

	  vec4  gl_ClipPlane[gl_MaxClipPlanes];

	  struct gl_PointParameters 
	  {
		 float size;
		 float sizeMin;
		 float sizeMax;
		 float fadeThresholdSize;
		 float distanceConstantAttenuation;
		 float distanceLinearAttenuation;
		 float distanceQuadraticAttenuation;
	  };
	  gl_PointParameters gl_Point;

	  struct gl_MaterialParameters 
	  {
		  vec4  emission; // Ecm
		  vec4  ambient; // Acm
		  vec4  diffuse; // Dcm
		  vec4  specular; // Scm
		  float shininess; // Srm
	  };
	  gl_MaterialParameters gl_FrontMaterial;
	  gl_MaterialParameters gl_BackMaterial;

	  struct gl_LightSourceParameters
	  {
		  vec4  ambient; // Acli
		  vec4  diffuse; // Dcli
		  vec4  specular; // Scli
		  vec4  position; // Ppli
		  vec4  halfVector; // Derived: Hi
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
	  gl_LightSourceParameters gl_LightSource[gl_MaxLights];

	  struct gl_LightModelParameters 
	  {
	      vec4  ambient; // Acs
	  };
	  gl_LightModelParameters gl_LightModel;

	  struct gl_LightModelProducts 
	  {
	      vec4  sceneColor; // Derived. Ecm + Acm * Acs
	  };
	  gl_LightModelProducts gl_FrontLightModelProduct;
	  gl_LightModelProducts gl_BackLightModelProduct;

	  struct gl_LightProducts {
	      vec4  ambient; // Acm * Acli
		  vec4  diffuse; // Dcm * Dcli
		  vec4  specular; // Scm * Scli
	  };
	  gl_LightProducts gl_FrontLightProduct[gl_MaxLights];
	  gl_LightProducts gl_BackLightProduct[gl_MaxLights];
	  
	  vec4  gl_EyePlaneS[gl_MaxTextureCoords];
	  vec4  gl_EyePlaneT[gl_MaxTextureCoords];
	  vec4  gl_EyePlaneR[gl_MaxTextureCoords];
	  vec4  gl_EyePlaneQ[gl_MaxTextureCoords];
	  vec4  gl_ObjectPlaneS[gl_MaxTextureCoords];
	  vec4  gl_ObjectPlaneT[gl_MaxTextureCoords];
	  vec4  gl_ObjectPlaneR[gl_MaxTextureCoords];
	  vec4  gl_ObjectPlaneQ[gl_MaxTextureCoords];

	  struct vec4  gl_PMXFogParam
	  {
	      density * ONE_OVER_LN_TWO,
		  density * ONE_OVER_SQRT_LN_TWO,
		  -1.0/(end - start),
		  end/(end - satrt)
	  }		

	*/

	FFGEN_STATE_MODELVIEWMATRIX,                           /* GLSLBV_MODELVIEWMATRIX                 */
	FFGEN_STATE_PROJECTMATRIX,                             /* GLSLBV_PROJECTMATRIX                   */
	FFGEN_STATE_MODELVIEWPROJECTIONMATRIX,                 /* GLSLBV_MODELVIEWPROJECTIONMATRIX       */
	FFGEN_STATE_TEXTUREMATRIX,                             /* GLSLBV_TEXTUREMATRIX                   */
	FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSE,           /* GLSLBV_MODELVIEWMATRIXINVERSETRANSPOSE */
	FFGEN_STATE_MATRIXPALETTEINDEXCLAMP,				   /* GLSLBV_MATRIXPALETTEINDEXCLAMP         */
	FFGEN_STATE_MODELVIEWMATRIXPALETTE,                    /* GLSLBV_SPECIAL_MODELVIEWMATRIXPALETTEE */
	FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSEPALETTE,    /* GLSLBV_SPECIAL_MODELVIEWMATRIXINVERSETRANSPOSEPALETTE */
	FFGEN_STATE_CLIPPLANE,                                 /* GLSLBV_CLIPPLANE                       */
	FFGEN_STATE_POINT,                                     /* GLSLBV_POINT                           */
	FFGEN_STATE_FRONTMATERIAL,                             /* GLSLBV_FRONTMATERIAL                   */
	FFGEN_STATE_BACKMATERIAL,                              /* GLSLBV_BACKMATERIAL                    */
	FFGEN_STATE_LIGHTSOURCE,                               /* GLSLBV_LIGHTSOURCE                     */
	FFGEN_STATE_LIGHTSOURCE0,                              /* GLSLBV_PMXLIGHTSOURCE0                 */
	FFGEN_STATE_LIGHTSOURCE1,                              /* GLSLBV_PMXLIGHTSOURCE1                 */
	FFGEN_STATE_LIGHTSOURCE2,                              /* GLSLBV_PMXLIGHTSOURCE2                 */
	FFGEN_STATE_LIGHTSOURCE3,                              /* GLSLBV_PMXLIGHTSOURCE3                 */
	FFGEN_STATE_LIGHTSOURCE4,                              /* GLSLBV_PMXLIGHTSOURCE4                 */
	FFGEN_STATE_LIGHTSOURCE5,                              /* GLSLBV_PMXLIGHTSOURCE5                 */
	FFGEN_STATE_LIGHTSOURCE6,                              /* GLSLBV_PMXLIGHTSOURCE6                 */
	FFGEN_STATE_LIGHTSOURCE7,                              /* GLSLBV_PMXLIGHTSOURCE7                 */
	FFGEN_STATE_LIGHTMODEL,                                /* GLSLBV_LIGHTMODEL                      */
	FFGEN_STATE_FRONTLIGHTMODELPRODUCT,                    /* GLSLBV_FRONTLIGHTMODELPRODUCT          */
	FFGEN_STATE_BACKLIGHTMODELPRODUCT,                     /* GLSLBV_BACKLIGHTMODELPRODUCT           */
	FFGEN_STATE_FRONTLIGHTPRODUCT,                         /* GLSLBV_FRONTLIGHTPRODUCT               */
	FFGEN_STATE_BACKLIGHTPRODUCT,                          /* GLSLBV_BACKLIGHTPRODUCT                */
	FFGEN_STATE_EYEPLANES,                                 /* GLSLBV_EYEPLANES                       */
	FFGEN_STATE_EYEPLANET,                                 /* GLSLBV_EYEPLANET                       */
	FFGEN_STATE_EYEPLANER,                                 /* GLSLBV_EYEPLANER,                      */
	FFGEN_STATE_EYEPLANEQ,                                 /* GLSLBV_EYEPLANEQ                       */
	FFGEN_STATE_OBJECTPLANES,                              /* GLSLBV_OBJECTPLANES                    */
	FFGEN_STATE_OBJECTPLANET,                              /* GLSLBV_OBJECTPLANET                    */
	FFGEN_STATE_OBJECTPLANER,                              /* GLSLBV_OBJECTPLANER                    */
	FFGEN_STATE_OBJECTPLANEQ,                              /* GLSLBV_OBJECTPLANEQ                    */
	FFGEN_STATE_PMXFOGPARAM,                               /* GLSLBV_PMXFOGPARAM                     */
	FFGEN_STATE_PMXPOINTSIZE,                              /* GLSLBV_PMXPOINTSIZE                    */



	FFTNL_NUM_REGISTER_DESCRIPTIONS,

	/* 
		Geometry shader built-in state
	*/

#if defined(OGL_LINESTIPPLE)
	/* 
		int commWordPosition;
		int syncWord - sa used for shader sync. shader internal use
		int inVertexStride;
		float halfLineWidth;
		vec2 halfViewport;
		vec2 oneOverHalfViewport;
	*/
	FFGEN_STATE_COMMWORDPOSITION,							/* cCommWordPos */
	FFGEN_STATE_SYNCWORD,									/* cSyncWord */
	FFGEN_STATE_INVERTEXSTRIDE,								/* cInVertexStride */
	FFGEN_STATE_HALFLINEWIDTH,								/* cHalfWidth */
	FFGEN_STATE_HALFVIEWPORT,								/* cHalfViewport */
	FFGEN_STATE_ONEOVERHALFVIEWPORT,						/* cOneOverHalfViewport */

	/*
		Built-in state for IMG_texten_line

		float lineWidth;

		vec2 glLineOffset[gl_MaxTextureCoords];
		vec2 glLineRepeat[gl_MaxTextureCoords];
	*/
	FFGEN_STATE_LINEWIDTH,									/* cLineWidth */
	FFGEN_STATE_LINEOFFSET,									/* cLineOffset */
	FFGEN_STATE_LINEREPEAT,									/* cLineRepeat */
	FFGEN_STATE_ACCUMUPARAMS,								/* cAccumParams, currently total line length and vertex count */
	
	/*
		float intersecParams([4] or [8] depending on join style)
		float mitreLimit;
		float roundCapConstants[6];
		float roundJoinContants[30];
	*/
	FFGEN_STATE_INTERSECPARAMS,								/* cIntersecParams */
	FFGEN_STATE_MITRELIMIT,									/* cMitreLimit */
	FFGEN_STATE_ROUNDCAPCONSTANTS,							/* cRoundCapConstants */

	/*
		int glLStripSegmentCount;
	*/
	FFGEN_STATE_LSTRIPSEGMENTCOUNT,							/* cLStripSegmentCount */
	
	/* 
		Built-in state for texture matrix 

		mat4 textureMatrix0;
		mat4 textureMatrix1;
		mat4 textureMatrix2;
		mat4 textureMatrix3;
		mat4 textureMatrix4;
		mat4 textureMatrix5;
		mat4 textureMatrix6;
		mat4 textureMatrix7;
	*/
	FFGEN_STATE_TEXTUREMATRIX0,
	FFGEN_STATE_TEXTUREMATRIX1,
	FFGEN_STATE_TEXTUREMATRIX2,
	FFGEN_STATE_TEXTUREMATRIX3,
	FFGEN_STATE_TEXTUREMATRIX4,
	FFGEN_STATE_TEXTUREMATRIX5,
	FFGEN_STATE_TEXTUREMATRIX6,
	FFGEN_STATE_TEXTUREMATRIX7,
#endif /* #if defined(OGL_CARNAV_EXTS) || defined(OGL_LINESTIPPLE) */

} FFGenRegDesc;

/* 
	Internal use enum defines 
*/
typedef enum 
{
	FFGEN_WDF_0 = 1,
	FFGEN_WDF_1 = 2,
	FFGEN_WDF_2 = 3,
	FFGEN_WDF_3 = 4,
} FFGenWDFStatus;

	
/* 
	Register struct 
*/
typedef struct FFGenReg_TAG
{
	UseasmRegType		 eType;					/* Useasm reg type */
	IMG_UINT32			 uOffset;				/* Offset */
	IMG_UINT32			 uSizeInDWords;			/* Total size */
	FFGenRegDesc		 eBindingRegDesc;		/* Binding ID */
	FFGenWDFStatus		 eWDFStatus;			/* Internal use only */
	IMG_UINT32			 uIndex;				/* Internal use, can be USEREG_INDEX_NONE, USEREG_INDEX_L and USEREG_INDEX_H */
#if defined(STANDALONE) || defined(DEBUG)
	IMG_CHAR			 acDesc[MAX_DESC_SIZE];
#endif

#if defined(OGLES1_MODULE) && defined(FFGEN_UNIFLEX)
	IMG_UINT32 *pui32SrcOffset;
	IMG_UINT32 *pui32DstOffset;
	IMG_UINT32 ui32ConstantCount;
#endif
} FFGenReg;

typedef struct FFGenRegList_TAG
{
	FFGenReg			 *psReg;

	struct FFGenRegList_TAG *psNext;
	struct FFGenRegList_TAG *psPrev;

} FFGenRegList;


typedef enum FFGenProgDesc_TAG
{
	FFGENPD_NEW,
	FFGENPD_CURRENT,
	FFGENPD_EXISTING,
} FFGenProgDesc;

#if defined(OGL_LINESTIPPLE)
typedef struct FFGenGeoExtra_TAG
{
	/* Memory address adjustment when loading memory constants */
	IMG_INT32			iSAAddressAdjust;			/* Memory address adjustment when loading memory constants */

	/* These are used only for geometry shader */
	IMG_UINT32			uOutVertexStride;
	IMG_UINT32			uNumPartitionsRequired;

	IMG_UINT32			uProgramStart;
	IMG_UINT32			uTotalNumInstructions;
	IMG_UINT32			uNumTemporaryRegisters;
	IMG_UINT32			*puUSECode;

}FFGenGeoExtra;
#endif




#if defined(OGLES1_MODULE)

typedef struct
{
	/*
		Contains flags with information about the program.
	*/
	IMG_UINT32				bUSEPerInstanceMode;

	/*
		Count of instructions in program.
	*/
	IMG_UINT32				ui32InstructionCount;

	/*
		Array of the instructions generated for the program.
	*/
	IMG_UINT32				*pui32Instructions;

	/*
		Count of the primary attribute registers used in the program.
	*/
	IMG_UINT32				ui32PrimaryAttributeCount;

	/*
		Count of the secondary attribute registers used in the program.
	*/
	IMG_UINT32				ui32SecondaryAttributeCount;

	/*
		Count of the temporary registers used in the program.
	*/
	IMG_UINT32				ui32TemporaryRegisterCount;

	/*
		Count of the memory constants used in the program.
	*/
	IMG_UINT32				ui32MemoryConstantCount;

	/*
		Offset for memory constants used in the program.
	*/
	IMG_INT16				iSAAddressAdjust;			

} FFGEN_PROGRAM_DETAILS;

#endif /* defined(OGLES1_MODULE) */



/*
	Generated program output
*/
typedef struct FFGenProgram_TAG
{
#if defined(OGLES1_MODULE)

	FFGEN_PROGRAM_DETAILS *psFFGENProgramDetails;

#if defined(FFGEN_UNIFLEX)
	UNIFLEX_INST *psUniFlexInst;

	IMG_UINT32 *pui32UFConstantData;
	IMG_UINT32 *pui32UFConstantDest;
	IMG_UINT32 ui32NumUFConstants;
	IMG_UINT32 ui32MaxNumUFConstants;
#endif /* defined(FFGEN_UNIFLEX) */

#else /* defined(OGLES1_MODULE) */
	UNIFLEX_HW *psUniFlexHW;
#endif /* defined(OGLES1_MODULE) */


	/* 
		Register usage for constants, inputs and outputs
	*/
	FFGenRegList		  *psConstantsList;
	FFGenRegList		  *psInputsList;
	FFGenRegList		  *psOutputsList;

	/* 
		Texture coordinate dimension information 
	*/
	IMG_UINT32			  uNumTexCoordUnits;
	IMG_UINT32			  auOutputTexDimensions[FFTNLGEN_MAX_NUM_TEXTURE_UNITS];

	/* 
		Constant size
	*/
	IMG_UINT32			  uMemoryConstantsSize;		/* Memory constant size, in dwords */
	IMG_UINT32			  uMemConstBaseAddrSAReg;	/* Memory constant base address reg */

	IMG_UINT32			  uSecAttribSize;			/* Secondary attributes , in dwords */
	IMG_UINT32			  uSecAttribStart;

	/* 
		Might need to be removed 
	*/
	IMG_UINT32           uHashValue;
	IMG_UINT32           uRefCount;
#if !defined(OGLES1_MODULE)
	IMG_UINT32           uID;
#endif /* !defined(OGLES1_MODULE) */

#if defined(OGL_LINESTIPPLE)
	FFGenGeoExtra		 *psGeoExtra;
#endif

#ifdef FFGEN_UNIFLEX
	UNIFLEX_INST* psUniFlexInstructions;
#endif

	struct FFGenProgram_TAG *psNext;
	struct FFGenProgram_TAG *psPrev;

} FFGenProgram;

typedef IMG_PVOID (IMG_CALLCONV *FFGEN_MALLOCFN) (IMG_HANDLE hHandle, IMG_UINT32 uSize);
typedef IMG_PVOID (IMG_CALLCONV *FFGEN_CALLOCFN) (IMG_HANDLE hHandle, IMG_UINT32 uSize);
typedef IMG_PVOID (IMG_CALLCONV *FFGEN_REALLOCFN)(IMG_HANDLE hHandle, IMG_VOID *pvData, IMG_UINT32 uSize);
typedef IMG_VOID  (IMG_CALLCONV *FFGEN_FREEFN)   (IMG_HANDLE hHandle, IMG_VOID *pvData);
typedef IMG_VOID  (IMG_CALLCONV *FFGEN_PRINTFN)  (const IMG_CHAR* pszFormat, ...) IMG_FORMAT_PRINTF(1, 2);


/* 
	Create FFGen context 
*/

IMG_INTERNAL IMG_VOID* IMG_CALLCONV FFGenCreateContext(IMG_HANDLE		hClientHandle,
													   const IMG_CHAR	*pszDumpFileName,
													   FFGEN_MALLOCFN	pfnMalloc,
													   FFGEN_CALLOCFN	pfnCalloc,
													   FFGEN_REALLOCFN	pfnRealloc,
													   FFGEN_FREEFN		pfnFree,
													   FFGEN_PRINTFN	pfnPrint);
/*
	Destroy FFGen context
*/

IMG_INTERNAL IMG_VOID IMG_CALLCONV FFGenDestroyContext(IMG_VOID *pvFFGenContext);

/* 
	Generate TNL program according to TNL gen description, 
*/
IMG_INTERNAL FFGenProgram* IMG_CALLCONV FFGenGenerateTNLProgram(IMG_VOID		*pvFFGenContext,
																FFTNLGenDesc	*psFFTNLGenDesc);


/* 
	Free a FF program generated by FFGenGenerateTNLProgram or FFGenGenerateGEOProgram
*/
IMG_INTERNAL IMG_VOID IMG_CALLCONV FFGenFreeProgram(IMG_VOID		*pvFFGenContext,
													FFGenProgram	*psFFGenProgram);


/* 
	Search through the list of TNL programs generated previously, return it immediately if found. 
	If not found, generate a new one, add it to the list and then return. 
*/
IMG_INTERNAL FFGenProgram* IMG_CALLCONV FFGenGetTNLProgram(IMG_VOID			*pvFFGenContext,
														   FFTNLGenDesc		*psFFTNLGenDesc,
														   FFGenProgDesc	*peProgDesc);


#if defined(OGL_LINESTIPPLE)

#define FFGEO_MAX_NUM_TEXCOORD_UNITS 10
#define FFGEO_MAX_NUM_CLIP_PLANES 6

/* 
	Line cape style for line tessellation when generating geometry shader 
*/
typedef enum FFGEOLineCapStyleTAG
{
	FFGEO_LCS_NONE		= 0,	/* Standard OpenGL lines */
	FFGEO_LCS_BUTT		= 1,	/* Butt */
	FFGEO_LCS_ROUND		= 2,	/* Round */
	FFGEO_LCS_SQUARE	= 3,	/* Square */

}FFGEOLineCapStyle;

typedef enum FFGEOLineJoinStyleTAG
{
	FFGEO_LJS_NONE,			/* No join */
	FFGEO_LJS_BEVEL,		/* Bevel */
	FFGEO_LJS_ROUND,		/* Round */
	FFGEO_LJS_MITRE			/* Mitre */
}FFGEOLineJoinStyle;

typedef enum FFGEOAttributeMaskTAG
{
	FFGEO_ATTRIB_POS,
	FFGEO_ATTRIB_COL,
	FFGEO_ATTRIB_SECCOL,
	FFGEO_ATTRIB_TC0,
	FFGEO_ATTRIB_TC1,
	FFGEO_ATTRIB_TC2,
	FFGEO_ATTRIB_TC3,
	FFGEO_ATTRIB_TC4,
	FFGEO_ATTRIB_TC5,
	FFGEO_ATTRIB_TC6,
	FFGEO_ATTRIB_TC7,
	FFGEO_ATTRIB_TC8,
	FFGEO_ATTRIB_TC9,
	FFGEO_ATTRIB_FOG,
	FFGEO_ATTRIB_PTSIZE,
	FFGEO_ATTRIB_CLIP0,
	FFGEO_ATTRIB_CLIP1,
	FFGEO_ATTRIB_CLIP2,
	FFGEO_ATTRIB_CLIP3,
	FFGEO_ATTRIB_CLIP4,
	FFGEO_ATTRIB_CLIP5,

	/*
		line width should only be present for input attribute mask
	*/
	FFGEO_ATTRIB_LWIDTH,

	FFGEO_ATTRIB_TYPES
}FFGEOAttributeMask;

/* 
	Enables for geometry shader 
*/
typedef enum FFGEOEnablesTAG
{
	FFGEO_LINE_TESSELLATE			= 0x1,
	FFGEO_ENABLE_SHADER_SYNC		= 0x2,
}FFGEOEnables;

typedef enum FFGEOLineTexGenModeTAG
{
	FFGEO_LTGM_PIXEL		= 0x0,
	FFGEO_LTGM_PARAMETRIC	= 0x1,
}FFGEOLineTexGenMode;


typedef enum FFGEOLinePrimTypeTAG
{
	FFGEO_LPT_LINES			= 0x0,
	FFGEO_LPT_LINE_STRIP	= 0x1,
	FFGEO_LPT_LINE_LOOP		= 0x2,
}FFGEOLinePrimType;

#define FFGEO_TEXGEN_COORD_SIZE 2

/* 
	Description for generation of geometry shader 
*/
typedef struct FFGenGSDescTAG
{
	/* FFGEO enables */
	FFGEOEnables			eGEOEnables;

	/* Input and output attribute masks */
	FFGEOAttributeMask		uInputAttribMask;
	FFGEOAttributeMask		uOutputAttribMask;
	IMG_UINT8				auTexCoordDims[ROUND_BYTE_ARRAY_TO_DWORD(FFGEO_MAX_NUM_TEXCOORD_UNITS)];

	/* line cap style and join style */
	FFGEOLineCapStyle		eLineCapStyle;
	FFGEOLineJoinStyle		eLineJoinStyle;
	FFGEOLinePrimType		eLinePrimType;

	/* texgen parameters */
	IMG_UINT32				uLineTexGenUnitEnables;
	IMG_UINT32				uLineTexGenAccumulateUnitEnables;
	IMG_UINT8				aeLineTexGenMode[ROUND_BYTE_ARRAY_TO_DWORD(FFGEO_MAX_NUM_TEXCOORD_UNITS)][2];
	IMG_UINT8				aubLineTexGenCoordMask[ROUND_BYTE_ARRAY_TO_DWORD(FFGEO_MAX_NUM_TEXCOORD_UNITS)];

	/* Which tex coords should be transformed by the texture matrices */
	IMG_UINT32				uEnabledTextureMatrices;

	/* Secondary attribute setup */
	IMG_UINT32				uSecAttrConstBaseAddressReg;
	IMG_UINT32				uSecAttrStart;
	IMG_UINT32				uSecAttrEnd;
	IMG_UINT32				uSecAttrAllOther;

	/* Code generation method */
	FFCodeGenMethod			eCodeGenMethod;

}FFGEOGenDesc;

/* 
	Generate GEO program according to GEO gen description, 
*/
IMG_INTERNAL FFGenProgram* IMG_CALLCONV FFGenGenerateGEOProgram(IMG_VOID		*pvFFGenContext,
																FFGEOGenDesc	*psFFGEOGenDesc);

#endif /* #if defined(OGL_CARNAV_EXTS) || defined(OGL_LINESTIPPLE)) */

#endif /* __gl_ffgen_h_ */

/******************************************************************************
 End of file (ffgen.h)
******************************************************************************/
