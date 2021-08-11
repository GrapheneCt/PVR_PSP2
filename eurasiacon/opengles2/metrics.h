/**************************************************************************
* Name         : metrics.h
* Author       : BCB
* Created      : 14/12/2005
*
* Copyright    : 2005-2006 by Imagination Technologies Limited. All rights reserved.
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
* $Date: 2011/01/27 12:30:18 $ $Revision: 1.54 $
* $Log: metrics.h $

**************************************************************************/
#ifndef _METRICS_
#define _METRICS_

#if defined (__cplusplus)
extern "C" {
#endif

#if defined(TIMING) || defined(DEBUG)


extern IMG_UINT32 ui32TextureMemCurrent;
extern IMG_UINT32  ui32TextureMemHWM;


IMG_BOOL InitMetrics(GLES2Context *gc);
IMG_VOID OutputMetrics(GLES2Context *gc);
IMG_VOID GetFrameTime(GLES2Context *gc);


#define GLES2_TIMER_DUMMY							0

#define GLES2_TIMER_PREPARE_TO_DRAW_TIME			1
#define GLES2_TIMER_TOTAL_FRAME_TIME				2
#define GLES2_TIMER_WAITING_FOR_3D_TIME				3

#define GLES2_TIMER_TEXTURE_ALLOCATE_TIME			4
#define GLES2_TIMER_TEXTURE_GHOST_LOAD_TIME			5
#define GLES2_TIMER_TEXTURE_TRANSLATE_LOAD_TIME		6
#define GLES2_TIMER_TEXTURE_READBACK_TIME			7 





#define GLES2_TIMER_MTE_STATE_COUNT					24
#define GLES2_TIMER_VDM_CTRL_STATE_COUNT			25

#define GLES2_TIMER_STATE_EMIT_TIME					26

#define GLES2_TIMER_VALIDATE_TIME					27
#define GLES2_TIMER_KICK_3D							28

#define GLES2_TIMER_SWAP_BUFFERS					29

#define GLES2_TIMER_NAMES_ARRAY						30

#define GLES2_TIMER_FRAME_RESOURCE_MANAGER_OP		31
#define GLES2_TIMER_FRAME_RESOURCE_MANAGER_WAIT		32

#define GLES2_TIMER_CODE_HEAP_TIME					33

#define GLES2_TIMER_SGXKICKTA_TIME					34
#define GLES2_TIMER_WAITING_FOR_TA_TIME				35


#define GLES2_TIMER_VERTEX_DATA_COPY				36
#define GLES2_TIMER_INDEX_DATA_GENERATE_COPY		37

#define GLES2_TIMER_VERTEX_DATA_COUNT				38
#define GLES2_TIMER_INDEX_DATA_COUNT				39

#define GLES2_TIMER_PDS_VERT_DATA_COUNT				40
#define GLES2_TIMER_PDS_FRAG_DATA_COUNT				41
#define GLES2_TIMER_USSE_FRAG_DATA_COUNT			42

#define GLES2_TIMER_WRITEPDSPIXELSHADERPROGRAM_TIME	43
#define GLES2_TIMER_WRITEPDSVERTEXSHADERPROGRAM_TIME 44
#define GLES2_TIMER_PATCHPDSVERTEXSHADERPROGRAM_TIME 45
#define GLES2_TIMER_WRITESAFRAGPROGRAM_TIME			46
#define GLES2_TIMER_WRITESAVERTPROGRAM_TIME			47

#define GLES2_TIMER_WRITEMTESTATE_TIME				48
#define GLES2_TIMER_WRITEMTESTATECOPYPROGRAMS_TIME	49
#define GLES2_TIMER_WRITEVDMCONTROLSTREAM_TIME		50

#define GLES2_TIMER_EMIT_BUGFIX_TIME				51

#define GLES2_TIMER_PDSVARIANT_HIT_COUNT			52
#define GLES2_TIMER_PDSVARIANT_MISS_COUNT			53

#define GLES2_TIMER_USECODEHEAP_FRAG_TIME			54
#define GLES2_TIMER_USECODEHEAP_FRAG_COUNT			55
#define GLES2_TIMER_USECODEHEAP_VERT_TIME			56
#define GLES2_TIMER_USECODEHEAP_VERT_COUNT			57
#define GLES2_TIMER_PDSCODEHEAP_FRAG_TIME			58
#define GLES2_TIMER_PDSCODEHEAP_FRAG_COUNT			59

#define GLES2_TIMER_SETUP_ATTRIB_STREAMS_TIME	    60
#define GLES2_TIMER_SETUP_ATTRIB_POINTER_TIME		61
#define GLES2_TIMER_SETUP_TEXTURESTATE_TIME			62
#define GLES2_TIMER_SETUP_VERTEXSHADER_TIME			63
#define GLES2_TIMER_SETUP_RENDERSTATE_TIME			64
#define GLES2_TIMER_SETUP_FRAGMENTSHADER_TIME		65

#define GLES2_TIMER_MTE_COPY_COUNT                  66

/* Mask incoming 'mode' parameter to keep it in range of valid(+spacer) values */
#define GLES2_TIMER_ARRAY_MODE_MASK		         	0x7

#define GLES2_TIMER_ARRAY_POINTS_TIME               70
#define GLES2_TIMER_ARRAY_LINES_TIME                71
#define GLES2_TIMER_ARRAY_LINE_LOOP_TIME            72
#define GLES2_TIMER_ARRAY_LINE_STRIP_TIME           73
#define GLES2_TIMER_ARRAY_TRIANGLES_TIME            74
#define GLES2_TIMER_ARRAY_TRIANGLE_STRIP_TIME       75
#define GLES2_TIMER_ARRAY_TRIANGLE_FAN_TIME         76

#define GLES2_TIMER_ARRAY_SPACER1_TIME         		77

#define GLES2_TIMER_ELEMENT_POINTS_TIME             78
#define GLES2_TIMER_ELEMENT_LINES_TIME              79
#define GLES2_TIMER_ELEMENT_LINE_LOOP_TIME          80
#define GLES2_TIMER_ELEMENT_LINE_STRIP_TIME         81
#define GLES2_TIMER_ELEMENT_TRIANGLES_TIME          82
#define GLES2_TIMER_ELEMENT_TRIANGLE_STRIP_TIME     83
#define GLES2_TIMER_ELEMENT_TRIANGLE_FAN_TIME       84

#define GLES2_TIMER_ARRAY_SPACER2_TIME         		85

#define GLES2_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME		    	  86
#define GLES2_TIMER_SETUP_VAO_ATTRIB_POINTERS_TIME			      87
#define GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME      88
#define GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_REST_TIME 89
#define GLES2_TIMER_PATCH_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME      90
#define GLES2_TIMER_GENERATE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME   91

#define GLES2_TIMER_SGXKICKTA_BINDFRAMEBUFFER_COUNT					92
#define GLES2_TIMER_SGXKICKTA_FLUSHFRAMEBUFFER_COUNT				93
#define GLES2_TIMER_SGXKICKTA_BUFDATA_COUNT				94

/* entry point times */
#define GLES2_TIMES_glActiveTexture					140
#define GLES2_TIMES_glAttachShader					141
#define GLES2_TIMES_glBindAttribLocation			142
#define GLES2_TIMES_glBindBuffer					143
#define GLES2_TIMES_glBindFramebuffer				144
#define GLES2_TIMES_glBindRenderbuffer				145
#define GLES2_TIMES_glBindTexture					146
#define GLES2_TIMES_glBlendColor					147
#define GLES2_TIMES_glBlendEquation					148
#define GLES2_TIMES_glBlendEquationSeparate			149
#define GLES2_TIMES_glBlendFunc						150
#define GLES2_TIMES_glBlendFuncSeparate				151
#define GLES2_TIMES_glBufferData					152
#define GLES2_TIMES_glBufferSubData					153
#define GLES2_TIMES_glCheckFramebufferStatus		154
#define GLES2_TIMES_glClear							155
#define GLES2_TIMES_glClearColor					156
#define GLES2_TIMES_glClearDepthf					157
#define GLES2_TIMES_glClearStencil					158
#define GLES2_TIMES_glColorMask						159
#define GLES2_TIMES_glCompileShader					160
#define GLES2_TIMES_glCompressedTexImage2D			161
#define GLES2_TIMES_glCompressedTexSubImage2D		162
#define GLES2_TIMES_glCopyTexImage2D				163
#define GLES2_TIMES_glCopyTexSubImage2D				164
#define GLES2_TIMES_glCreateProgram					165
#define GLES2_TIMES_glCreateShader					166
#define GLES2_TIMES_glCullFace						167
#define GLES2_TIMES_glDeleteBuffers					168
#define GLES2_TIMES_glDeleteFramebuffers			169
#define GLES2_TIMES_glDeleteProgram					170
#define GLES2_TIMES_glDeleteRenderbuffers			171
#define GLES2_TIMES_glDeleteShader					172
#define GLES2_TIMES_glDeleteTextures				173
#define GLES2_TIMES_glDetachShader					174
#define GLES2_TIMES_glDepthFunc						175
#define GLES2_TIMES_glDepthMask						176
#define GLES2_TIMES_glDepthRangef					177
#define GLES2_TIMES_glDisable						178
#define GLES2_TIMES_glDisableVertexAttribArray		179
#define GLES2_TIMES_glDrawArrays					180
#define GLES2_TIMES_glDrawElements					181
#define GLES2_TIMES_glEnable						182
#define GLES2_TIMES_glEnableVertexAttribArray		183
#define GLES2_TIMES_glFinish						184
#define GLES2_TIMES_glFlush							185
#define GLES2_TIMES_glFramebufferRenderbuffer		186
#define GLES2_TIMES_glFramebufferTexture2D			187
#define GLES2_TIMES_glFramebufferTexture3DOES		188
#define GLES2_TIMES_glFrontFace						189
#define GLES2_TIMES_glGenBuffers					190
#define GLES2_TIMES_glGenerateMipmap				191
#define GLES2_TIMES_glGenFramebuffers				192
#define GLES2_TIMES_glGenRenderbuffers				193
#define GLES2_TIMES_glGenTextures					194
#define GLES2_TIMES_glGetActiveAttrib				195
#define GLES2_TIMES_glGetActiveUniform				196
#define GLES2_TIMES_glGetAttachedShaders			197
#define GLES2_TIMES_glGetAttribLocation				198
#define GLES2_TIMES_glGetBooleanv					199
#define GLES2_TIMES_glGetBufferParameteriv			200
#define GLES2_TIMES_glGetBufferPointervOES			201
#define GLES2_TIMES_glGetError						202
#define GLES2_TIMES_glGetFloatv						203
#define GLES2_TIMES_glGetFramebufferAttachmentParameteriv	204
#define GLES2_TIMES_glGetIntegerv					205
#define GLES2_TIMES_glGetProgramiv					206
#define GLES2_TIMES_glGetProgramInfoLog				207
#define GLES2_TIMES_glGetRenderbufferParameteriv	208
#define GLES2_TIMES_glGetShaderiv					209
#define GLES2_TIMES_glGetShaderInfoLog				210
#define GLES2_TIMES_glGetShaderPrecisionFormat		211
#define GLES2_TIMES_glGetShaderSource				212
#define GLES2_TIMES_glGetString						213
#define GLES2_TIMES_glGetTexParameteriv				214
#define GLES2_TIMES_glGetTexParameterfv				215
#define GLES2_TIMES_glGetUniformfv					216
#define GLES2_TIMES_glGetUniformiv					217
#define GLES2_TIMES_glGetUniformLocation			218
#define GLES2_TIMES_glGetVertexAttribfv				219
#define GLES2_TIMES_glGetVertexAttribiv				220
#define GLES2_TIMES_glGetVertexAttribPointerv		221
#define GLES2_TIMES_glHint							222
#define GLES2_TIMES_glIsBuffer						223
#define GLES2_TIMES_glIsEnabled						224
#define GLES2_TIMES_glIsFramebuffer					225
#define GLES2_TIMES_glIsProgram						226
#define GLES2_TIMES_glIsRenderbuffer				227
#define GLES2_TIMES_glIsShader						228
#define GLES2_TIMES_glIsTexture						229
#define GLES2_TIMES_glLineWidth						230
#define GLES2_TIMES_glLinkProgram					231
#define GLES2_TIMES_glMapBuffer						232
#define GLES2_TIMES_glPixelStorei					233
#define GLES2_TIMES_glPolygonOffset					234
#define GLES2_TIMES_glReadPixels					235
#define GLES2_TIMES_glReleaseShaderCompiler			236
#define GLES2_TIMES_glRenderbufferStorage			237
#define GLES2_TIMES_glSampleCoverage				238
#define GLES2_TIMES_glScissor						239
#define GLES2_TIMES_glShaderBinary					240
#define GLES2_TIMES_glShaderSource					241
#define GLES2_TIMES_glStencilFunc					242
#define GLES2_TIMES_glStencilFuncSeparate			243
#define GLES2_TIMES_glStencilMask					244
#define GLES2_TIMES_glStencilMaskSeparate			245
#define GLES2_TIMES_glStencilOp						246
#define GLES2_TIMES_glStencilOpSeparate				247
#define GLES2_TIMES_glTexImage2D					248
#define GLES2_TIMES_glTexParameteri					249
#define GLES2_TIMES_glTexParameterf					250
#define GLES2_TIMES_glTexParameteriv				251
#define GLES2_TIMES_glTexParameterfv				252
#define GLES2_TIMES_glTexSubImage2D					253
#define GLES2_TIMES_glUniform1i						254
#define GLES2_TIMES_glUniform2i						255
#define GLES2_TIMES_glUniform3i						256
#define GLES2_TIMES_glUniform4i						257
#define GLES2_TIMES_glUniform1f						258
#define GLES2_TIMES_glUniform2f						259
#define GLES2_TIMES_glUniform3f						260
#define GLES2_TIMES_glUniform4f						261
#define GLES2_TIMES_glUniform1iv					262
#define GLES2_TIMES_glUniform2iv					263
#define GLES2_TIMES_glUniform3iv					264
#define GLES2_TIMES_glUniform4iv					265
#define GLES2_TIMES_glUniform1fv					266
#define GLES2_TIMES_glUniform2fv					267
#define GLES2_TIMES_glUniform3fv					268
#define GLES2_TIMES_glUniform4fv					269
#define GLES2_TIMES_glUniformMatrix2fv				270
#define GLES2_TIMES_glUniformMatrix3fv				271
#define GLES2_TIMES_glUniformMatrix4fv				272
#define GLES2_TIMES_glUnmapBuffer					273
#define GLES2_TIMES_glUseProgram					274
#define GLES2_TIMES_glValidateProgram				275
#define GLES2_TIMES_glVertexAttrib1f				276
#define GLES2_TIMES_glVertexAttrib2f				277
#define GLES2_TIMES_glVertexAttrib3f				278
#define GLES2_TIMES_glVertexAttrib4f				279
#define GLES2_TIMES_glVertexAttrib1fv				280
#define GLES2_TIMES_glVertexAttrib2fv				281
#define GLES2_TIMES_glVertexAttrib3fv				282
#define GLES2_TIMES_glVertexAttrib4fv				283
#define GLES2_TIMES_glVertexAttribPointer			284
#define GLES2_TIMES_glViewport						285

#define GLES2_TIMES_glEGLImageTargetTexture2DOES			286
#define GLES2_TIMES_glEGLImageTargetRenderbufferStorageOES	287

#define GLES2_TIMES_glMultiDrawArrays						288
#define GLES2_TIMES_glMultiDrawElements						289

#define GLES2_TIMES_glGetTexStreamDeviceAttributeiv			290
#define GLES2_TIMES_glTexBindStream							291
#define GLES2_TIMES_glGetTexStreamDeviceName				292

#define GLES2_TIMES_glGetSupportedInternalFormatsOES		293

#define GLES2_TIMES_glGetProgramBinaryOES					294
#define GLES2_TIMES_glProgramBinaryOES						295

#define GLES2_TIMES_glBindVertexArrayOES                    296
#define GLES2_TIMES_glDeleteVertexArraysOES                 297
#define GLES2_TIMES_glGenVertexArraysOES                    298
#define GLES2_TIMES_glIsVertexArrayOES                      299

#define GLES2_TIMES_glDiscardFramebufferEXT					300

#define GLES2_NUM_TIMERS					(GLES2_TIMES_glDiscardFramebufferEXT + 1)


#define GLES2_CALLS(X)				    PVR_MTR_CALLS(gc->asTimes[X])

#define GLES2_TIME_PER_CALL(X)		    PVR_MTR_TIME_PER_CALL(gc->asTimes[X], gc->fCPUSpeed)

#define GLES2_MAX_TIME(X)		        PVR_MTR_MAX_TIME(gc->asTimes[X], gc->fCPUSpeed)
									
#define GLES2_PIXELS_PER_CALL(X)    	PVR_MTR_PIXELS_PER_CALL(gc->asTimes[X])

#define GLES2_TIME_PER_FRAME(X, Y)		PVR_MTR_TIME_PER_FRAME_GENERIC(gc->asTimes[X], Y, gc->fCPUSpeed)

#define GLES2_PIXELS_PER_FRAME(X, Y)	PVR_MTR_PIXELS_PER_FRAME_GENERIC(gc->asTimes[X], Y)

#define GLES2_METRIC_PER_FRAME(X, Y)	PVR_MTR_METRIC_PER_FRAME_GENERIC(gc->asTimes[X], Y)

#define GLES2_PARAM_PER_FRAME(X, Y)		PVR_MTR_PARAM_PER_FRAME_GENERIC(gc->asTimes[X], Y)

#define GLES2_CHECK_BETWEEN_START_END_FRAME	PVR_MTR_CHECK_BETWEEN_START_END_FRAME(gc->sAppHints, gc->asTimes[GLES2_TIMER_SWAP_BUFFERS])

#define GLES2_TIME_RESET(X)             PVR_MTR_TIME_RESET(gc->asTimes[X])

#define GLES2_TIME_START(X)             PVR_MTR_TIME_START(gc->asTimes[X], gc->sAppHints, gc->asTimes[GLES2_TIMER_SWAP_BUFFERS])

#define GLES2_TIME_SUSPEND(X)           PVR_MTR_TIME_SUSPEND(gc->asTimes[X])

#define GLES2_TIME_RESUME(X)            PVR_MTR_TIME_RESUME(gc->asTimes[X])

#define GLES2_TIME_STOP(X)              PVR_MTR_TIME_STOP(gc->asTimes[X], gc->sAppHints, gc->asTimes[GLES2_TIMER_SWAP_BUFFERS])

#define GLES2_TIME_RESET_SUM(X)         PVR_MTR_TIME_RESET_SUM(gc->asTimes[X])
								 								
#define GLES2_INC_PERFRAME_COUNT(X)     PVR_MTR_INC_PERFRAME_COUNT(gc->asTimes[X])

#define GLES2_RESET_PERFRAME_COUNT(X)   PVR_MTR_RESET_PERFRAME_COUNT(gc->asTimes[X])

#define GLES2_DECR_COUNT(Y)   	   	    PVR_MTR_DECR_COUNT(gc->asTimes[X])

#define GLES2_INC_PIXEL_COUNT(X, Y)     PVR_MTR_INC_PIXEL_COUNT(gc->asTimes[X], Y)
	 
#if defined(TIMING) && (TIMING_LEVEL < 1)

#define GLES2_TIME1_START(X)
#define GLES2_TIME1_STOP(X)
#define GLES2_INC_COUNT(Y,X)

#else /* defined(TIMING) && (TIMING_LEVEL < 1)*/ 
                         
#define GLES2_TIME1_START(X)	PVR_MTR_TIME_START(gc->asTimes[X], gc->sAppHints, gc->asTimes[GLES2_TIMER_SWAP_BUFFERS])
#define GLES2_TIME1_STOP(X)		PVR_MTR_TIME_STOP(gc->asTimes[X], gc->sAppHints, gc->asTimes[GLES2_TIMER_SWAP_BUFFERS])
#define GLES2_INC_COUNT(Y,X)    PVR_MTR_INC_COUNT(Y, gc->asTimes[Y], X, gc->sAppHints, gc->asTimes[GLES2_TIMER_SWAP_BUFFERS], GLES2_TIMER_SWAP_BUFFERS)

#endif /* defined(TIMING) && (TIMING_LEVEL < 1)*/ 



#else /* !(defined (TIMING) || defined (DEBUG)) */

#define NUM_TIMERS

#define GLES2_TIME_RESET(X)
#define GLES2_TIME_START(X)
#define GLES2_TIME_SUSPEND(X)
#define GLES2_TIME_RESUME(X)
#define GLES2_TIME_STOP(X)
#define GLES2_TIME1_START(X)
#define GLES2_TIME1_STOP(X)
#define GLES2_INC_COUNT(X,Y)
#define GLES2_DECR_COUNT(X)
#define GLES2_INC_PIXEL_COUNT(X, Y)
#define GLES2_INC_PERFRAME_COUNT(X)
#define GLES2_RESET_PERFRAME_COUNT(X)
#define GLES2_TIME_RESET_SUM(X)

#define GLES2_RESET_FRAME()
#define GLES2_INC_POLY_COUNT(X)

#endif /* defined (TIMING) || defined (DEBUG) */

#if defined(__cplusplus)
}
#endif

#endif /* _METRICS_ */



