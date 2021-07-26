/**************************************************************************
* Name         : metrics.h
*
* Copyright    : 2003-2006 by Imagination Technologies Limited. All rights reserved.
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
* $Log: metrics.h $ 
*
*  --- Revision Logs Removed --- 
**************************************************************************/
#ifndef _METRICS_
#define _METRICS_

#if defined (__cplusplus)
extern "C" {
#endif

#if defined(TIMING) || defined(DEBUG)


extern IMG_UINT32 ui32TextureMemCurrent;
extern IMG_UINT32  ui32TextureMemHWM;


IMG_BOOL InitMetrics(GLES1Context *gc);
IMG_VOID OutputMetrics(GLES1Context *gc);
IMG_VOID GetFrameTime(GLES1Context *gc);

#define METRICS_GROUP_ENABLED                 0x00001FFF 

/* 
   Timer groups 
   - each new timer has to belong to a group
   - a timer is enabled only if its group is enabled (at build time)
*/
#define GLES1_METRICS_GROUP_ENTRYPOINTS       0x00000001
#define GLES1_METRICS_GROUP_VALIDATESTATE     0x00000002
#define GLES1_METRICS_GROUP_EMITSTATE         0x00000004
#define GLES1_METRICS_GROUP_TEXTURE           0x00000008
#define GLES1_METRICS_GROUP_USECODEHEAP       0x00000010
#define GLES1_METRICS_GROUP_SGXIF             0x00000020
#define GLES1_METRICS_GROUP_DATACOPY          0x00000040
#define GLES1_METRICS_GROUP_BUFFER            0x00000080
#define GLES1_METRICS_GROUP_PDSVARIANT        0x00000100
#define GLES1_METRICS_GROUP_VPGEN             0x00000200
#define GLES1_METRICS_GROUP_DRAWFUNCTIONS     0x00000400
#define GLES1_METRICS_GROUP_MISC              0x00000800
#define GLES1_METRICS_GROUP_SWAPBUFFER        0x00001000

/*
  Timer defines
*/
#define GLES1_TIMER_DUMMY						    0

#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_SGXIF)

#define GLES1_TIMER_PREPARE_TO_DRAW_TIME			1
#define GLES1_TIMER_TOTAL_FRAME_TIME				2
#define GLES1_TIMER_WAITING_FOR_3D_TIME				3
	
#define GLES1_TIMER_SGXKICKTA_TIME                  4
#define GLES1_TIMER_WAITING_FOR_TA_TIME             5
#define GLES1_TIMER_KICK_3D                         6

#else /*(METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_SGXIF)*/

#define GLES1_TIMER_PREPARE_TO_DRAW_TIME			0
#define GLES1_TIMER_TOTAL_FRAME_TIME				0
#define GLES1_TIMER_WAITING_FOR_3D_TIME				0
#define GLES1_TIMER_SGXKICKTA_TIME                  0
#define GLES1_TIMER_WAITING_FOR_TA_TIME             0
#define GLES1_TIMER_KICK_3D                         0

#endif /*(METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_SGXIF)*/



#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_TEXTURE)

#define GLES1_TIMER_TEXTURE_ALLOCATE_TIME			7
#define GLES1_TIMER_TEXTURE_GHOST_LOAD_TIME			8
#define GLES1_TIMER_TEXTURE_TRANSLATE_LOAD_TIME		9
#define GLES1_TIMER_TEXTURE_READBACK_TIME			10 

#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_TEXTURE) */

#define GLES1_TIMER_TEXTURE_ALLOCATE_TIME			0
#define GLES1_TIMER_TEXTURE_GHOST_LOAD_TIME			0
#define GLES1_TIMER_TEXTURE_TRANSLATE_LOAD_TIME		0
#define GLES1_TIMER_TEXTURE_READBACK_TIME			0 

#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_TEXTURE) */



#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_DATACOPY)

#define GLES1_TIMER_VERTEX_DATA_COPY				25
#define GLES1_TIMER_INDEX_DATA_GENERATE_COPY		26

#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_DATACOPY) */

#define GLES1_TIMER_VERTEX_DATA_COPY				0
#define GLES1_TIMER_INDEX_DATA_GENERATE_COPY		0

#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_DATACOPY) */



#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_BUFFER)

#define GLES1_TIMER_VDM_CTRL_STATE_COUNT			27
#define GLES1_TIMER_VERTEX_DATA_COUNT				28
#define GLES1_TIMER_INDEX_DATA_COUNT				29
#define GLES1_TIMER_PDS_VERT_DATA_COUNT				30
#define GLES1_TIMER_PDS_VERT_AUXILIARY_COUNT        31
#define GLES1_TIMER_MTE_COPY_COUNT					32
#define GLES1_TIMER_PDS_FRAG_DATA_COUNT				33
#define GLES1_TIMER_USSE_FRAG_DATA_COUNT			34

#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_BUFFER) */

#define GLES1_TIMER_VERTEX_DATA_COUNT				0
#define GLES1_TIMER_INDEX_DATA_COUNT				0
#define GLES1_TIMER_PDS_VERT_DATA_COUNT				0
#define GLES1_TIMER_PDS_VERT_AUXILIARY_COUNT        0
#define GLES1_TIMER_MTE_COPY_COUNT					0
#define GLES1_TIMER_PDS_FRAG_DATA_COUNT				0
#define GLES1_TIMER_USSE_FRAG_DATA_COUNT			0

#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_BUFFER) */



#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_SWAPBUFFER)

#define GLES1_TIMER_SWAP_BUFFERS_TIME					35
#define GLES1_TIMER_SWAP_BUFFERS_COUNT					36

#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_SWAPBUFFER)  */

#define GLES1_TIMER_SWAP_BUFFERS_TIME					0
#define GLES1_TIMER_SWAP_BUFFERS_COUNT					0

#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_SWAPBUFFER) */




#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_MISC)

#define GLES1_TIMER_MATRIXMATHS						37
#define GLES1_TIMER_NAMES_ARRAY						38
#define GLES1_TIMER_MTE_STATE_COUNT					39

#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_MISC)*/

#define GLES1_TIMER_MATRIXMATHS						0
#define GLES1_TIMER_NAMES_ARRAY						0
#define GLES1_TIMER_MTE_STATE_COUNT					0

#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_MISC)*/



#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_EMITSTATE)

#define GLES1_TIMER_STATE_EMIT_TIME						40

#define GLES1_TIMER_WRITEPDSPIXELSHADERPROGRAM_TIME		41
#define GLES1_TIMER_WRITEPDSVERTEXSHADERPROGRAM_TIME	42
#define GLES1_TIMER_WRITEPDSUSESHADERSAPROGRAM_TIME		43

#define GLES1_TIMER_WRITEMTESTATE_TIME					44
#define GLES1_TIMER_WRITEMTESTATECOPYPROGRAMS_TIME		45
#define GLES1_TIMER_SETUPSTATEUPDATE_TIME				46
#define GLES1_TIMER_WRITEVDMCONTROLSTREAM_TIME			47

#define GLES1_TIMER_EMIT_BUGFIX_TIME					48

#define GLES1_TIMER_WRITEPDSVERTEXAUXILIARY_TIME        49

#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_EMITSTATE)*/

#define GLES1_TIMER_STATE_EMIT_TIME						0
#define GLES1_TIMER_WRITEPDSPIXELSHADERPROGRAM_TIME		0
#define GLES1_TIMER_WRITEPDSVERTEXSHADERPROGRAM_TIME	0
#define GLES1_TIMER_WRITEPDSUSESHADERSAPROGRAM_TIME		0
#define GLES1_TIMER_WRITEMTESTATE_TIME					0
#define GLES1_TIMER_WRITEMTESTATECOPYPROGRAMS_TIME		0
#define GLES1_TIMER_SETUPSTATEUPDATE_TIME				0
#define GLES1_TIMER_WRITEVDMCONTROLSTREAM_TIME			0
#define GLES1_TIMER_EMIT_BUGFIX_TIME					0

#define GLES1_TIMER_WRITEPDSVERTEXAUXILIARY_TIME        0

#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_EMITSTATE)*/



#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_USECODEHEAP)

#define GLES1_TIMER_USECODEHEAP_FRAG_TIME				50
#define GLES1_TIMER_USECODEHEAP_FRAG_COUNT				51
#define GLES1_TIMER_USECODEHEAP_VERT_TIME				52
#define GLES1_TIMER_USECODEHEAP_VERT_COUNT				53
#define GLES1_TIMER_PDSCODEHEAP_FRAG_TIME				54
#define GLES1_TIMER_PDSCODEHEAP_FRAG_COUNT				55
#define GLES1_TIMER_CODE_HEAP_TIME                      56

#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_USECODEHEAP) */

#define GLES1_TIMER_USECODEHEAP_FRAG_TIME				0
#define GLES1_TIMER_USECODEHEAP_FRAG_COUNT				0
#define GLES1_TIMER_USECODEHEAP_VERT_TIME				0
#define GLES1_TIMER_USECODEHEAP_VERT_COUNT				0
#define GLES1_TIMER_PDSCODEHEAP_FRAG_TIME				0
#define GLES1_TIMER_PDSCODEHEAP_FRAG_COUNT				0
#define GLES1_TIMER_CODE_HEAP_TIME                      0

#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_USECODEHEAP) */



#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_VALIDATESTATE)

#define GLES1_TIMER_VALIDATE_TIME					    59 
#define GLES1_TIMER_SETUP_TEXTURESTATE_TIME				60
#define GLES1_TIMER_SETUP_STREAMS_TIME					61
#define GLES1_TIMER_UPDATEMVP_TIME						62
#define GLES1_TIMER_SETUP_VERTEXSHADER_TIME				63
#define GLES1_TIMER_SETUP_VERTEXCONSTANTS_TIME			64
#define GLES1_TIMER_SETUP_OUTPUTS_TIME					65
#define GLES1_TIMER_SETUP_VERTEXVARIANT_TIME			66
#define GLES1_TIMER_SETUP_RENDERSTATE_TIME				67
#define GLES1_TIMER_SETUP_FRAGMENTSHADER_TIME			68
#define GLES1_TIMER_FFGEN_GENERATION_TIME				69
#define GLES1_TIMER_USC_TIME							70

#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_VALIDATESTATE)  */

#define GLES1_TIMER_VALIDATE_TIME					    0
#define GLES1_TIMER_SETUP_TEXTURESTATE_TIME				0
#define GLES1_TIMER_SETUP_STREAMS_TIME					0
#define GLES1_TIMER_UPDATEMVP_TIME						0
#define GLES1_TIMER_SETUP_VERTEXSHADER_TIME				0
#define GLES1_TIMER_SETUP_VERTEXCONSTANTS_TIME			0
#define GLES1_TIMER_SETUP_OUTPUTS_TIME					0
#define GLES1_TIMER_SETUP_VERTEXVARIANT_TIME			0
#define GLES1_TIMER_SETUP_RENDERSTATE_TIME				0
#define GLES1_TIMER_SETUP_FRAGMENTSHADER_TIME			0
#define GLES1_TIMER_FFGEN_GENERATION_TIME				0
#define GLES1_TIMER_USC_TIME							0

#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_VALIDATESTATE)  */



#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_PDSVARIANT)

#define GLES1_TIMER_PDSVARIANT_HIT_COUNT				80
#define GLES1_TIMER_PDSVARIANT_MISS_COUNT				81

#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_PDSVARIANT) */

#define GLES1_TIMER_PDSVARIANT_HIT_COUNT				0
#define GLES1_TIMER_PDSVARIANT_MISS_COUNT				0

#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_PDSVARIANT) */

 
#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_DRAWFUNCTIONS)

/* Mask incoming 'mode' parameter to keep it in range of valid(+spacer) values */
#define GLES1_TIMER_ARRAY_MODE_MASK		         	0x7

#define GLES1_TIMER_ARRAY_POINTS_TIME               120
#define GLES1_TIMER_ARRAY_LINES_TIME                121
#define GLES1_TIMER_ARRAY_LINE_LOOP_TIME            122
#define GLES1_TIMER_ARRAY_LINE_STRIP_TIME           123
#define GLES1_TIMER_ARRAY_TRIANGLES_TIME            124
#define GLES1_TIMER_ARRAY_TRIANGLE_STRIP_TIME       125
#define GLES1_TIMER_ARRAY_TRIANGLE_FAN_TIME         126

#define GLES1_TIMER_ARRAY_SPACER1_TIME         		127

#define GLES1_TIMER_ELEMENT_POINTS_TIME             128
#define GLES1_TIMER_ELEMENT_LINES_TIME              129
#define GLES1_TIMER_ELEMENT_LINE_LOOP_TIME          130
#define GLES1_TIMER_ELEMENT_LINE_STRIP_TIME         131
#define GLES1_TIMER_ELEMENT_TRIANGLES_TIME          132
#define GLES1_TIMER_ELEMENT_TRIANGLE_STRIP_TIME     133
#define GLES1_TIMER_ELEMENT_TRIANGLE_FAN_TIME       134

#define GLES1_TIMER_ARRAY_SPACER2_TIME         		135

#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_DRAWFUNCTIONS)  */

#define GLES1_TIMER_ARRAY_MASK		         		0

#define GLES1_TIMER_ARRAY_POINTS_TIME               0
#define GLES1_TIMER_ARRAY_LINES_TIME                0
#define GLES1_TIMER_ARRAY_LINE_LOOP_TIME            0
#define GLES1_TIMER_ARRAY_LINE_STRIP_TIME           0
#define GLES1_TIMER_ARRAY_TRIANGLES_TIME            0
#define GLES1_TIMER_ARRAY_TRIANGLE_STRIP_TIME       0
#define GLES1_TIMER_ARRAY_TRIANGLE_FAN_TIME         0

#define GLES1_TIMER_ARRAY_SPACER1_TIME         		0

#define GLES1_TIMER_ELEMENT_POINTS_TIME             0
#define GLES1_TIMER_ELEMENT_LINES_TIME              0
#define GLES1_TIMER_ELEMENT_LINE_LOOP_TIME          0
#define GLES1_TIMER_ELEMENT_LINE_STRIP_TIME         0
#define GLES1_TIMER_ELEMENT_TRIANGLES_TIME          0
#define GLES1_TIMER_ELEMENT_TRIANGLE_STRIP_TIME     0
#define GLES1_TIMER_ELEMENT_TRIANGLE_FAN_TIME       0
	
#define GLES1_TIMER_ARRAY_SPACER2_TIME         		0

#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_DRAWFUNCTIONS)  */



#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_DRAWFUNCTIONS)

/* Mask incoming 'mode' parameter to keep it in range of valid(+spacer) values */

#define GLES1_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME		    	  136
#define GLES1_TIMER_SETUP_VAO_ATTRIB_POINTERS_TIME			      137
#define GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME      138
#define GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_GENERATE_OR_PATCH_TIME      139


#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_DRAWFUNCTIONS)  */

#define GLES1_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME		    	  0
#define GLES1_TIMER_SETUP_VAO_ATTRIB_POINTERS_TIME			      0
#define GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME      0
#define GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_GENERATE_OR_PATCH_TIME      0

#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_DRAWFUNCTIONS)  */



#if (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_ENTRYPOINTS)

/* entry point times */
#define GLES1_TIMES_glActiveTexture					140
#define GLES1_TIMES_glAlphaFunc						141
#define GLES1_TIMES_glAlphaFuncx					142
#define GLES1_TIMES_glBindTexture					143
#define GLES1_TIMES_glBlendFunc						144
#define GLES1_TIMES_glClear							145
#define GLES1_TIMES_glClearColor					146
#define GLES1_TIMES_glClearColorx					147
#define GLES1_TIMES_glClearDepthf					148
#define GLES1_TIMES_glClearDepthx 					149
#define GLES1_TIMES_glClearStencil					150
#define GLES1_TIMES_glClientActiveTexture			151
#define GLES1_TIMES_glColor4f						152
#define GLES1_TIMES_glColor4x						153
#define GLES1_TIMES_glColorMask 					154
#define GLES1_TIMES_glColorPointer 					155
#define GLES1_TIMES_glCompressedTexImage2D 			156
#define GLES1_TIMES_glCompressedTexSubImage2D 		157
#define GLES1_TIMES_glCopyTexImage2D 				158
#define GLES1_TIMES_glCopyTexSubImage2D 			159
#define GLES1_TIMES_glCullFace 						160
#define GLES1_TIMES_glDeleteTextures 				161
#define GLES1_TIMES_glDepthFunc 					162
#define GLES1_TIMES_glDepthMask 					163
#define GLES1_TIMES_glDepthRangef 					164
#define GLES1_TIMES_glDepthRangex 					165
#define GLES1_TIMES_glDisable 						166
#define GLES1_TIMES_glDisableClientState 			167
#define GLES1_TIMES_glDrawArrays 					168
#define GLES1_TIMES_glDrawElements 					169
#define GLES1_TIMES_glEnable 						170
#define GLES1_TIMES_glEnableClientState 			171
#define GLES1_TIMES_glFinish 						172
#define GLES1_TIMES_glFlush 						173
#define GLES1_TIMES_glFogf 							174
#define GLES1_TIMES_glFogfv 						175
#define GLES1_TIMES_glFogx 							176
#define GLES1_TIMES_glFogxv 						177
#define GLES1_TIMES_glFrontFace 					178
#define GLES1_TIMES_glFrustumf 						179
#define GLES1_TIMES_glFrustumx 						180
#define GLES1_TIMES_glGenTextures 					181
#define GLES1_TIMES_glGetError						182
#define GLES1_TIMES_glGetIntegerv 					183
#define GLES1_TIMES_glGetString 					184
#define GLES1_TIMES_glHint 							185
#define GLES1_TIMES_glLightModelf 					186
#define GLES1_TIMES_glLightModelfv 					187
#define GLES1_TIMES_glLightModelx 					188
#define GLES1_TIMES_glLightModelxv 					189
#define GLES1_TIMES_glLightf						190
#define GLES1_TIMES_glLightfv 						191
#define GLES1_TIMES_glLightx 						192
#define GLES1_TIMES_glLightxv 						193
#define GLES1_TIMES_glLineWidth 					194
#define GLES1_TIMES_glLineWidthx 					195
#define GLES1_TIMES_glLoadIdentity 					196
#define GLES1_TIMES_glLoadMatrixf 					197
#define GLES1_TIMES_glLoadMatrixx 					198
#define GLES1_TIMES_glLogicOp						199
#define GLES1_TIMES_glMaterialf 					200
#define GLES1_TIMES_glMaterialfv					201
#define GLES1_TIMES_glMaterialx 					202
#define GLES1_TIMES_glMaterialxv					203
#define GLES1_TIMES_glMatrixMode 					204
#define GLES1_TIMES_glMultMatrixf 					205
#define GLES1_TIMES_glMultMatrixx 					206
#define GLES1_TIMES_glMultiTexCoord4f 				207
#define GLES1_TIMES_glMultiTexCoord4x 				208
#define GLES1_TIMES_glNormal3f 						209
#define GLES1_TIMES_glNormal3x 						210
#define GLES1_TIMES_glNormalPointer 				211
#define GLES1_TIMES_glOrthof 						212
#define GLES1_TIMES_glOrthox 						213
#define GLES1_TIMES_glPixelStorei 					214
#define GLES1_TIMES_glPointSize 					215
#define GLES1_TIMES_glPointSizex 					216
#define GLES1_TIMES_glPolygonOffset 				217
#define GLES1_TIMES_glPolygonOffsetx 				218
#define GLES1_TIMES_glPopMatrix 					219
#define GLES1_TIMES_glPushMatrix 					220
#define GLES1_TIMES_glReadPixels 					221
#define GLES1_TIMES_glRotatef 						222
#define GLES1_TIMES_glRotatex 						223
#define GLES1_TIMES_glSampleCoverage 				224
#define GLES1_TIMES_glSampleCoveragex 				225
#define GLES1_TIMES_glScalef 						226
#define GLES1_TIMES_glScalex						227
#define GLES1_TIMES_glScissor 						228
#define GLES1_TIMES_glShadeModel 					229
#define GLES1_TIMES_glStencilFunc 					230
#define GLES1_TIMES_glStencilMask 					231
#define GLES1_TIMES_glStencilOp 					232
#define GLES1_TIMES_glTexCoordPointer				233
#define GLES1_TIMES_glTexEnvf 						234
#define GLES1_TIMES_glTexEnvfv 						235
#define GLES1_TIMES_glTexEnvx 						236
#define GLES1_TIMES_glTexEnvxv 						237
#define GLES1_TIMES_glTexImage2D 					238
#define GLES1_TIMES_glTexParameterf					239
#define GLES1_TIMES_glTexParameterx 				240
#define GLES1_TIMES_glTexSubImage2D 				241
#define GLES1_TIMES_glTranslatef 					242
#define GLES1_TIMES_glTranslatex 					243
#define GLES1_TIMES_glVertexPointer 				244
#define GLES1_TIMES_glViewport 						245

#define GLES1_TIMES_glClipPlanef					246
#define GLES1_TIMES_glClipPlanex					247

#define GLES1_TIMES_glQueryMatrixx					248
#define GLES1_TIMES_glColorMaterial					249

#define GLES1_TIMES_glGetTexStreamDeviceAttributeiv				250
#define GLES1_TIMES_glTexBindStream								251
#define GLES1_TIMES_glGetTexStreamDeviceName					252

#define GLES1_TIMES_glBindBuffer								253
#define GLES1_TIMES_glBufferData								254
#define GLES1_TIMES_glBufferSubData								255
#define GLES1_TIMES_glDeleteBuffers								256
#define GLES1_TIMES_glGenBuffers								257

#define GLES1_TIMES_glGetBufferParameteriv						258
#define GLES1_TIMES_glGetMaterialfv								259
#define GLES1_TIMES_glGetMaterialxv								260
#define GLES1_TIMES_glGetLightfv								261
#define GLES1_TIMES_glGetLightxv								262
#define GLES1_TIMES_glGetTexEnviv								263
#define GLES1_TIMES_glGetTexEnvfv								264
#define GLES1_TIMES_glGetTexEnvxv								265
#define GLES1_TIMES_glGetClipPlanef								266
#define GLES1_TIMES_glGetClipPlanex								267
#define GLES1_TIMES_glGetTexParameteriv							268
#define GLES1_TIMES_glGetTexParameterfv							269
#define GLES1_TIMES_glGetTexParameterxv							270
#define GLES1_TIMES_glGetFixedv									271
#define GLES1_TIMES_glGetFloatv									272
#define GLES1_TIMES_glGetBooleanv								273
#define GLES1_TIMES_glGetPointerv								274
#define GLES1_TIMES_glIsEnabled									275
#define GLES1_TIMES_glIsBuffer									276
#define GLES1_TIMES_glIsTexture									277

#define GLES1_TIMES_glPointParameterf							278
#define GLES1_TIMES_glPointParameterfv							279
#define GLES1_TIMES_glPointParameterx							280
#define GLES1_TIMES_glPointParameterxv							281

#define GLES1_TIMES_glPointSizePointerOES						282

#define GLES1_TIMES_glTexEnvi									283
#define GLES1_TIMES_glTexEnviv									284

#define GLES1_TIMES_glTexParameteri								285
#define GLES1_TIMES_glTexParameterfv							286
#define GLES1_TIMES_glTexParameteriv							287
#define GLES1_TIMES_glTexParameterxv							288

#define GLES1_TIMES_glColor4ub									289

#define GLES1_TIMES_glMatrixIndexPointerOES						290
#define GLES1_TIMES_glWeightPointerOES							291
#define GLES1_TIMES_glCurrentPaletteMatrixOES					292
#define GLES1_TIMES_glLoadPaletteFromModelViewMatrixOES			293

#define GLES1_TIMES_glDrawTexsOES								294
#define GLES1_TIMES_glDrawTexiOES								295
#define GLES1_TIMES_glDrawTexfOES								296
#define GLES1_TIMES_glDrawTexxOES								297
#define GLES1_TIMES_glDrawTexsvOES								298
#define GLES1_TIMES_glDrawTexivOES								299
#define GLES1_TIMES_glDrawTexfvOES								300
#define GLES1_TIMES_glDrawTexxvOES								301


#define GLES1_TIMES_glIsRenderbufferOES							302
#define GLES1_TIMES_glBindRenderbufferOES						303
#define GLES1_TIMES_glDeleteRenderbuffersOES					304
#define GLES1_TIMES_glGenRenderbuffersOES						305
#define GLES1_TIMES_glRenderbufferStorageOES					306
#define GLES1_TIMES_glGetRenderbufferParameterivOES				307
#define GLES1_TIMES_glGetRenderbufferStorageFormatsivOES		308
#define GLES1_TIMES_glIsFramebufferOES							309
#define GLES1_TIMES_glBindFramebufferOES						310
#define GLES1_TIMES_glDeleteFramebuffersOES						311
#define GLES1_TIMES_glGenFramebuffersOES						312
#define GLES1_TIMES_glCheckFramebufferStatusOES					313
#define GLES1_TIMES_glFramebufferTexture2DOES					314
#define GLES1_TIMES_glFramebufferTexture3DOES					315
#define GLES1_TIMES_glFramebufferRenderbufferOES				316
#define GLES1_TIMES_glGetFramebufferAttachmentParameterivOES	317
#define GLES1_TIMES_glGenerateMipmapOES							318

#define GLES1_TIMES_glBlendFuncSeparate							319
#define GLES1_TIMES_glBlendEquation								320
#define GLES1_TIMES_glBlendEquationSeparate						321

#define GLES1_TIMES_glTexGeniOES								322
#define GLES1_TIMES_glTexGenivOES								323
#define GLES1_TIMES_glTexGenfOES								324
#define GLES1_TIMES_glTexGenfvOES								325
#define GLES1_TIMES_glTexGenxOES								326
#define GLES1_TIMES_glTexGenxvOES								327
#define GLES1_TIMES_glGetTexGenivOES							328
#define GLES1_TIMES_glGetTexGenfvOES							329
#define GLES1_TIMES_glGetTexGenxvOES							330
#define GLES1_TIMES_glMapBufferOES                              331
#define GLES1_TIMES_glUnmapBufferOES                            332
#define GLES1_TIMES_glGetBufferPointervOES						333

#define GLES1_TIMES_glEGLImageTargetTexture2DOES				334
#define GLES1_TIMES_glEGLImageTargetRenderbufferStorageOES		335

#define GLES1_TIMES_glMultiDrawArrays							336
#define GLES1_TIMES_glMultiDrawElements							337

#define GLES1_TIMES_glBindVertexArrayOES                        338
#define GLES1_TIMES_glDeleteVertexArraysOES                     339
#define GLES1_TIMES_glGenVertexArraysOES                        340
#define GLES1_TIMES_glIsVertexArrayOES                          341


#else /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_ENTRYPOINTS) */


/* entry point times */
#define GLES1_TIMES_glActiveTexture					0
#define GLES1_TIMES_glAlphaFunc						0
#define GLES1_TIMES_glAlphaFuncx					0
#define GLES1_TIMES_glBindTexture					0
#define GLES1_TIMES_glBlendFunc						0
#define GLES1_TIMES_glClear							0
#define GLES1_TIMES_glClearColor					0
#define GLES1_TIMES_glClearColorx					0
#define GLES1_TIMES_glClearDepthf					0
#define GLES1_TIMES_glClearDepthx 					0
#define GLES1_TIMES_glClearStencil					0
#define GLES1_TIMES_glClientActiveTexture			0
#define GLES1_TIMES_glColor4f						0
#define GLES1_TIMES_glColor4x						0
#define GLES1_TIMES_glColorMask 					0
#define GLES1_TIMES_glColorPointer 					0
#define GLES1_TIMES_glCompressedTexImage2D 			0
#define GLES1_TIMES_glCompressedTexSubImage2D 		0
#define GLES1_TIMES_glCopyTexImage2D 				0
#define GLES1_TIMES_glCopyTexSubImage2D 			0
#define GLES1_TIMES_glCullFace 						0
#define GLES1_TIMES_glDeleteTextures 				0
#define GLES1_TIMES_glDepthFunc 					0
#define GLES1_TIMES_glDepthMask 					0
#define GLES1_TIMES_glDepthRangef 					0
#define GLES1_TIMES_glDepthRangex 					0
#define GLES1_TIMES_glDisable 						0
#define GLES1_TIMES_glDisableClientState 			0
#define GLES1_TIMES_glDrawArrays 					0
#define GLES1_TIMES_glDrawElements 					0
#define GLES1_TIMES_glEnable 						0
#define GLES1_TIMES_glEnableClientState 			0
#define GLES1_TIMES_glFinish 						0
#define GLES1_TIMES_glFlush 						0
#define GLES1_TIMES_glFogf 							0
#define GLES1_TIMES_glFogfv 						0
#define GLES1_TIMES_glFogx 							0
#define GLES1_TIMES_glFogxv 						0
#define GLES1_TIMES_glFrontFace 					0
#define GLES1_TIMES_glFrustumf 						0
#define GLES1_TIMES_glFrustumx 						0
#define GLES1_TIMES_glGenTextures 					0
#define GLES1_TIMES_glGetError						0
#define GLES1_TIMES_glGetIntegerv 					0
#define GLES1_TIMES_glGetString 					0
#define GLES1_TIMES_glHint 							0
#define GLES1_TIMES_glLightModelf 					0
#define GLES1_TIMES_glLightModelfv 					0
#define GLES1_TIMES_glLightModelx 					0
#define GLES1_TIMES_glLightModelxv 					0
#define GLES1_TIMES_glLightf						0
#define GLES1_TIMES_glLightfv 						0
#define GLES1_TIMES_glLightx 						0
#define GLES1_TIMES_glLightxv 						0
#define GLES1_TIMES_glLineWidth 					0
#define GLES1_TIMES_glLineWidthx 					0
#define GLES1_TIMES_glLoadIdentity 					0
#define GLES1_TIMES_glLoadMatrixf 					0
#define GLES1_TIMES_glLoadMatrixx 					0
#define GLES1_TIMES_glLogicOp						0
#define GLES1_TIMES_glMaterialf 					0
#define GLES1_TIMES_glMaterialfv					0
#define GLES1_TIMES_glMaterialx 					0
#define GLES1_TIMES_glMaterialxv					0
#define GLES1_TIMES_glMatrixMode 					0
#define GLES1_TIMES_glMultMatrixf 					0
#define GLES1_TIMES_glMultMatrixx 					0
#define GLES1_TIMES_glMultiTexCoord4f 				0
#define GLES1_TIMES_glMultiTexCoord4x 				0
#define GLES1_TIMES_glNormal3f 						0
#define GLES1_TIMES_glNormal3x 						0
#define GLES1_TIMES_glNormalPointer 				0
#define GLES1_TIMES_glOrthof 						0
#define GLES1_TIMES_glOrthox 						0
#define GLES1_TIMES_glPixelStorei 					0
#define GLES1_TIMES_glPointSize 					0
#define GLES1_TIMES_glPointSizex 					0
#define GLES1_TIMES_glPolygonOffset 				0
#define GLES1_TIMES_glPolygonOffsetx 				0
#define GLES1_TIMES_glPopMatrix 					0
#define GLES1_TIMES_glPushMatrix 					0
#define GLES1_TIMES_glReadPixels 					0
#define GLES1_TIMES_glRotatef 						0
#define GLES1_TIMES_glRotatex 						0
#define GLES1_TIMES_glSampleCoverage 				0
#define GLES1_TIMES_glSampleCoveragex 				0
#define GLES1_TIMES_glScalef 						0
#define GLES1_TIMES_glScalex						0
#define GLES1_TIMES_glScissor 						0
#define GLES1_TIMES_glShadeModel 					0
#define GLES1_TIMES_glStencilFunc 					0
#define GLES1_TIMES_glStencilMask 					0
#define GLES1_TIMES_glStencilOp 					0
#define GLES1_TIMES_glTexCoordPointer				0
#define GLES1_TIMES_glTexEnvf 						0
#define GLES1_TIMES_glTexEnvfv 						0
#define GLES1_TIMES_glTexEnvx 						0
#define GLES1_TIMES_glTexEnvxv 						0
#define GLES1_TIMES_glTexImage2D 					0
#define GLES1_TIMES_glTexParameterf					0
#define GLES1_TIMES_glTexParameterx 				0
#define GLES1_TIMES_glTexSubImage2D 				0
#define GLES1_TIMES_glTranslatef 					0
#define GLES1_TIMES_glTranslatex 					0
#define GLES1_TIMES_glVertexPointer 				0
#define GLES1_TIMES_glViewport 						0

#define GLES1_TIMES_glClipPlanef					0
#define GLES1_TIMES_glClipPlanex					0

#define GLES1_TIMES_glQueryMatrixx					0
#define GLES1_TIMES_glColorMaterial					0

#define GLES1_TIMES_glGetTexStreamDeviceAttributeiv	0
#define GLES1_TIMES_glTexBindStream					0
#define GLES1_TIMES_glGetTexStreamDeviceName		0

#define GLES1_TIMES_glBindBuffer					0
#define GLES1_TIMES_glBufferData					0
#define GLES1_TIMES_glBufferSubData					0
#define GLES1_TIMES_glDeleteBuffers					0
#define GLES1_TIMES_glGenBuffers					0

#define GLES1_TIMES_glGetBufferParameteriv			0
#define GLES1_TIMES_glGetMaterialfv					0
#define GLES1_TIMES_glGetMaterialxv					0
#define GLES1_TIMES_glGetLightfv					0
#define GLES1_TIMES_glGetLightxv					0
#define GLES1_TIMES_glGetTexEnviv					0
#define GLES1_TIMES_glGetTexEnvfv					0
#define GLES1_TIMES_glGetTexEnvxv					0
#define GLES1_TIMES_glGetClipPlanef					0
#define GLES1_TIMES_glGetClipPlanex					0
#define GLES1_TIMES_glGetTexParameteriv				0
#define GLES1_TIMES_glGetTexParameterfv				0
#define GLES1_TIMES_glGetTexParameterxv				0
#define GLES1_TIMES_glGetFixedv						0
#define GLES1_TIMES_glGetFloatv						0
#define GLES1_TIMES_glGetBooleanv					0
#define GLES1_TIMES_glGetPointerv					0
#define GLES1_TIMES_glIsEnabled						0
#define GLES1_TIMES_glIsBuffer						0
#define GLES1_TIMES_glIsTexture						0

#define GLES1_TIMES_glPointParameterf				0
#define GLES1_TIMES_glPointParameterfv				0
#define GLES1_TIMES_glPointParameterx				0
#define GLES1_TIMES_glPointParameterxv				0

#define GLES1_TIMES_glPointSizePointerOES			0

#define GLES1_TIMES_glTexEnvi						0
#define GLES1_TIMES_glTexEnviv						0

#define GLES1_TIMES_glTexParameteri					0
#define GLES1_TIMES_glTexParameterfv				0
#define GLES1_TIMES_glTexParameteriv				0
#define GLES1_TIMES_glTexParameterxv				0

#define GLES1_TIMES_glColor4ub						0

#define GLES1_TIMES_glMatrixIndexPointerOES				0
#define GLES1_TIMES_glWeightPointerOES					0
#define GLES1_TIMES_glCurrentPaletteMatrixOES			0
#define GLES1_TIMES_glLoadPaletteFromModelViewMatrixOES	0

#define GLES1_TIMES_glDrawTexsOES					0
#define GLES1_TIMES_glDrawTexiOES					0
#define GLES1_TIMES_glDrawTexfOES					0
#define GLES1_TIMES_glDrawTexxOES					0
#define GLES1_TIMES_glDrawTexsvOES					0
#define GLES1_TIMES_glDrawTexivOES					0
#define GLES1_TIMES_glDrawTexfvOES					0
#define GLES1_TIMES_glDrawTexxvOES					0


#define GLES1_TIMES_glIsRenderbufferOES							0
#define GLES1_TIMES_glBindRenderbufferOES						0
#define GLES1_TIMES_glDeleteRenderbuffersOES					0
#define GLES1_TIMES_glGenRenderbuffersOES						0
#define GLES1_TIMES_glRenderbufferStorageOES					0
#define GLES1_TIMES_glGetRenderbufferParameterivOES				0
#define GLES1_TIMES_glGetRenderbufferStorageFormatsivOES		0
#define GLES1_TIMES_glIsFramebufferOES							0
#define GLES1_TIMES_glBindFramebufferOES						0
#define GLES1_TIMES_glDeleteFramebuffersOES						0
#define GLES1_TIMES_glGenFramebuffersOES						0
#define GLES1_TIMES_glCheckFramebufferStatusOES					0
#define GLES1_TIMES_glFramebufferTexture2DOES					0
#define GLES1_TIMES_glFramebufferTexture3DOES					0
#define GLES1_TIMES_glFramebufferRenderbufferOES				0
#define GLES1_TIMES_glGetFramebufferAttachmentParameterivOES	0
#define GLES1_TIMES_glGenerateMipmapOES							0

#define GLES1_TIMES_glBlendFuncSeparate							0
#define GLES1_TIMES_glBlendEquation								0
#define GLES1_TIMES_glBlendEquationSeparate						0

#define GLES1_TIMES_glTexGeniOES								0
#define GLES1_TIMES_glTexGenivOES								0
#define GLES1_TIMES_glTexGenfOES								0
#define GLES1_TIMES_glTexGenfvOES								0
#define GLES1_TIMES_glTexGenxOES								0
#define GLES1_TIMES_glTexGenxvOES								0
#define GLES1_TIMES_glGetTexGenivOES							0
#define GLES1_TIMES_glGetTexGenfvOES							0
#define GLES1_TIMES_glGetTexGenxvOES							0

#define GLES1_TIMES_glMapBufferOES                              0
#define GLES1_TIMES_glUnmapBufferOES                            0
#define GLES1_TIMES_glGetBufferPointervOES						0

#define GLES1_TIMES_glEGLImageTargetTexture2DOES				0
#define GLES1_TIMES_glEGLImageTargetRenderbufferStorageOES		0

#define GLES1_TIMES_glMultiDrawArrays							0
#define GLES1_TIMES_glMultiDrawElements							0

#define GLES1_TIMES_glBindVertexArrayOES                        0
#define GLES1_TIMES_glDeleteVertexArraysOES                     0
#define GLES1_TIMES_glGenVertexArraysOES                        0
#define GLES1_TIMES_glIsVertexArrayOES                          0


#endif /* (METRICS_GROUP_ENABLED & GLES1_METRICS_GROUP_ENTRYPOINTS) */


#define GLES1_NUM_TIMERS                                342 /* (GLES1_TIMES_glIsVertexArrayOES + 1) */

/******************************************************
These are the new metrics, which basically 
redirect to those defined in pvr_metrics
*******************************************************/

#define GLES1_CALLS(X)				  PVR_MTR_CALLS(gc->asTimes[X])

#define GLES1_TIME_PER_CALL(X)		  PVR_MTR_TIME_PER_CALL(gc->asTimes[X], gc->fCPUSpeed)

#define GLES1_MAX_TIME(X)		      PVR_MTR_MAX_TIME(gc->asTimes[X], gc->fCPUSpeed)
									
#define GLES1_PIXELS_PER_CALL(X)	  PVR_MTR_PIXELS_PER_CALL(gc->asTimes[X])


#define GLES1_TIME_PER_FRAME(X)		  PVR_MTR_TIME_PER_FRAME(gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT], gc->asTimes[X], gc->fCPUSpeed)	

#define GLES1_PIXELS_PER_FRAME(X)	  PVR_MTR_PIXELS_PER_FRAME(gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT], gc->asTimes[X])

#define GLES1_METRIC_PER_FRAME(X)	  PVR_MTR_METRIC_PER_FRAME(gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT], gc->asTimes[X])

#define GLES1_PARAM_PER_FRAME(X)	  PVR_MTR_PARAM_PER_FRAME(gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT], gc->asTimes[X])	


#define GLES1_CHECK_BETWEEN_START_END_FRAME	PVR_MTR_CHECK_BETWEEN_START_END_FRAME(gc->sAppHints, gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT])


#define GLES1_TIME_RESET(X)           PVR_MTR_TIME_RESET(gc->asTimes[X])

#define GLES1_TIME_START(X)           {                                                                                           \
                                       int tmp = X;                                                                               \
	                                   if(tmp)                                                                                    \
                                       {                                                                                          \
	                                    PVR_MTR_TIME_START(gc->asTimes[X], gc->sAppHints, gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT])  \
									   }                                                                                          \
                                      }
	                                 

#define GLES1_SWAP_TIME_START(X)      PVR_MTR_SWAP_TIME_START(gc->asTimes[X])

#define GLES1_TIME_SUSPEND(X)         PVR_MTR_TIME_SUSPEND(gc->asTimes[X])

#define GLES1_TIME_RESUME(X)          PVR_MTR_TIME_RESUME(gc->asTimes[X])

#define GLES1_TIME_STOP(X)             {                                                                                          \
                                        int tmp = X;                                                                              \
	                                    if(tmp)                                                                                   \
                                        {                                                                                         \
                                         PVR_MTR_TIME_STOP(gc->asTimes[X], gc->sAppHints, gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT])  \
                                        }                                                                                         \
                                       }

#define GLES1_SWAP_TIME_STOP(X)       PVR_MTR_SWAP_TIME_STOP(gc->asTimes[X])

#define GLES1_TIME_RESET_SUM(X)       PVR_MTR_TIME_RESET_SUM(gc->asTimes[X]) 
								 								
#define GLES1_INC_PERFRAME_COUNT(X)	  PVR_MTR_INC_PERFRAME_COUNT(gc->asTimes[X])

#define GLES1_RESET_PERFRAME_COUNT(X) PVR_MTR_RESET_PERFRAME_COUNT(gc->asTimes[X])

#define GLES1_DECR_COUNT(Y)   	   	  PVR_MTR_DECR_COUNT(gc->asTimes[X])

#define GLES1_INC_PIXEL_COUNT(X, Y)   PVR_MTR_INC_PIXEL_COUNT(gc->asTimes[X], Y)
	 

#if defined(TIMING) && (TIMING_LEVEL < 1)

#define GLES1_TIME1_START(X)
#define GLES1_TIME1_STOP(X)
#define GLES1_INC_COUNT(Y,X)              

#else /* defined(TIMING) && (TIMING_LEVEL < 1)*/ 
                         
#define GLES1_TIME1_START(X)          PVR_MTR_TIME_START(gc->asTimes[X], gc->sAppHints, gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT])
#define GLES1_TIME1_STOP(X)           PVR_MTR_TIME_STOP(gc->asTimes[X], gc->sAppHints, gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT])
#define GLES1_INC_COUNT(Y,X)  	      {                                                                                            \
                                        int tmp = Y;                                                                               \
	                                    if(tmp)                                                                                    \
                                        {                                                                                          \
           PVR_MTR_INC_COUNT(Y, gc->asTimes[Y], X, gc->sAppHints, gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT], GLES1_TIMER_SWAP_BUFFERS_COUNT) \
										}                                                                                          \
                                      }
						
#endif /* defined(TIMING) && (TIMING_LEVEL < 1)*/ 



#else /* !(defined (TIMING) || defined (DEBUG)) */

#define NUM_TIMERS

#define GLES1_TIME_RESET(X)
#define GLES1_TIME_START(X)
#define GLES1_TIME_SUSPEND(X)
#define GLES1_TIME_RESUME(X)
#define GLES1_TIME_STOP(X)
#define GLES1_SWAP_TIME_START(X)
#define GLES1_SWAP_TIME_STOP(X)
#define GLES1_TIME1_START(X)
#define GLES1_TIME1_STOP(X)
#define GLES1_INC_COUNT(X,Y)
#define GLES1_DECR_COUNT(X)
#define GLES1_INC_PIXEL_COUNT(X, Y)
#define GLES1_INC_PERFRAME_COUNT(X)
#define GLES1_RESET_PERFRAME_COUNT(X)
#define GLES1_TIME_RESET_SUM(X)

#define GLES1_RESET_FRAME()
#define GLES1_INC_POLY_COUNT(X)

#endif /* defined (TIMING) || defined (DEBUG) */


#if defined(__cplusplus)
}
#endif

#endif /* _METRICS_ */

/**************************************************************************
 End of file (metrics.h)
**************************************************************************/


