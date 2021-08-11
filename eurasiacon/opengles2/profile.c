/******************************************************************************
 * Name         : profile.c
 *
 * Copyright    : 2006 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: profile.c $
 *
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#if defined(TIMING) || defined(DEBUG)

#include "context.h"

static IMG_CHAR* const pszFunctionNames[] =
{
	"glActiveTexture                         ",
	"glAttachShader                          ",
	"glBindAttribLocation                    ",
	"glBindBuffer                            ",
	"glBindFramebuffer                       ",
	"glBindRenderbuffer                      ",
	"glBindTexture                           ",
	"glBlendColor                            ",
	"glBlendEquation                         ",
	"glBlendEquationSeparate                 ",
	"glBlendFunc                             ",
	"glBlendFuncSeparate                     ",
	"glBufferData                            ",
	"glBufferSubData                         ",
	"glCheckFramebufferStatus                ",
	"glClear                                 ",
	"glClearColor                            ",
	"glClearDepthf                           ",
	"glClearStencil                          ",
	"glColorMask                             ",
	"glCompileShader                         ",
	"glCompressedTexImage2D                  ",
	"glCompressedTexSubImage2D               ",
	"glCopyTexImage2D                        ",
	"glCopyTexSubImage2D                     ",
	"glCreateProgram                         ",
	"glCreateShader                          ",
	"glCullFace                              ",
	"glDeleteBuffers                         ",
	"glDeleteFramebuffers                    ",
	"glDeleteProgram                         ",
	"glDeleteRenderbuffers                   ",
	"glDeleteShader                          ",
	"glDeleteTextures                        ",
	"glDetachShader                          ",
	"glDepthFunc                             ",
	"glDepthMask                             ",
	"glDepthRangef                           ",
	"glDisable                               ",
	"glDisableVertexAttribArray              ",
	"glDrawArrays                            ",
	"glDrawElements                          ",
	"glEnable                                ",
	"glEnableVertexAttribArray               ",
	"glFinish                                ",
	"glFlush                                 ",
	"glFramebufferRenderbuffer               ",
	"glFramebufferTexture2D                  ",
	"glFramebufferTexture3DOES               ",
	"glFrontFace                             ",
	"glGenBuffers                            ",
	"glGenerateMipmap					     ",
	"glGenFramebuffers                       ",
	"glGenRenderbuffers                      ",
	"glGenTextures                           ",
	"glGetActiveAttrib                       ",
	"glGetActiveUniform                      ",
	"glGetAttachedShaders                    ",
	"glGetAttribLocation                     ",
	"glGetBooleanv                           ",
	"glGetBufferParameteriv                  ",
	"glGetBufferPointervOES                  ",
	"glGetError                              ",
	"glGetFloatv                             ",
	"glGetFramebufferAttachmentParameteriv	 ",
	"glGetIntegerv                           ",
	"glGetProgramiv                          ",
	"glGetProgramInfoLog                     ",
	"glGetRenderbufferParameteriv			 ",
	"glGetShaderiv                           ",
	"glGetShaderInfoLog                      ",
	"glGetShaderPrecisionFormat              ",
	"glGetShaderSource                       ",
	"glGetString                             ",
	"glGetTexParameteriv                     ",
	"glGetTexParameterfv                     ",
	"glGetUniformfv                          ",
	"glGetUniformiv                          ",
	"glGetUniformLocation                    ",
	"glGetVertexAttribfv                     ",
	"glGetVertexAttribiv                     ",
	"glGetVertexAttribPointerv               ",
	"glHint                                  ",
	"glIsBuffer                              ",
	"glIsEnabled                             ",
	"glIsFramebuffer						 ",
	"glIsProgram                             ",
	"glIsRenderbuffer						 ",
	"glIsShader                              ",
	"glIsTexture                             ",
	"glLineWidth                             ",
	"glLinkProgram                           ",
	"glMapBufferOES                          ",
	"glPixelStorei                           ",
	"glPolygonOffset                         ",
	"glReadPixels                            ",
	"glReleaseShaderCompiler                 ",
	"glRenderbufferStorage                   ",
	"glSampleCoverage                        ",
	"glScissor                               ",
	"glShaderBinary                          ",
	"glShaderSource                          ",
	"glStencilFunc                           ",
	"glStencilFuncSeparate                   ",
	"glStencilMask                           ",
	"glStencilMaskSeparate                   ",
	"glStencilOp                             ",
	"glStencilOpSeparate                     ",
	"glTexImage2D                            ",
	"glTexParameteri                         ",
	"glTexParameterf                         ",
	"glTexParameteriv                        ",
	"glTexParameterfv                        ",
	"glTexSubImage2D                         ",
	"glUniform1i                             ",
	"glUniform2i                             ",
	"glUniform3i                             ",
	"glUniform4i                             ",
	"glUniform1f                             ",
	"glUniform2f                             ",
	"glUniform3f                             ",
	"glUniform4f                             ",
	"glUniform1iv                            ",
	"glUniform2iv                            ",
	"glUniform3iv                            ",
	"glUniform4iv                            ",
	"glUniform1fv                            ",
	"glUniform2fv                            ",
	"glUniform3fv                            ",
	"glUniform4fv                            ",
	"glUniformMatrix2fv                      ",
	"glUniformMatrix3fv                      ",
	"glUniformMatrix4fv                      ",
	"glUnmapBufferOES                        ",
	"glUseProgram                            ",
	"glValidateProgram                       ",
	"glVertexAttrib1f                        ",
	"glVertexAttrib2f                        ",
	"glVertexAttrib3f                        ",
	"glVertexAttrib4f                        ",
	"glVertexAttrib1fv                       ",
	"glVertexAttrib2fv                       ",
	"glVertexAttrib3fv                       ",
	"glVertexAttrib4fv                       ",
	"glVertexAttribPointer                   ",
	"glViewport                              ",

	"glEGLImageTargetTexture2DOES            ",
	"glEGLImageTargetRenderbufferStorageOES  ",

	"glMultiDrawArrays                       ",
	"glMultiDrawElements                     ",

	"glGetTexStreamDeviceAttributeiv	     ",
	"GLES2_TIMES_glTexBindStream			 ",	
	"GLES2_TIMES_glGetTexStreamDeviceName	 ",
	"GLES2_TIMES_glTexUnbindStream           ",

	"glGetSupportedInternalFormatsOES		 ",

	"glGetProgramBinaryOES                   ",	
	"glProgramBinaryOES                      ",

	"glBindVertexArrayOES                    ",
	"glDeleteVertexArraysOES                 ",
	"glGenVertexArraysOES                    ",
	"glIsVertexArrayOES                      ",

	"glDiscardFramebufferEXT                 "
};


#if defined(DEBUG) || (TIMING_LEVEL > 1)

static IMG_CHAR* const pszPrimNames[] =
{
	"GL_POINTS            ",
	"GL_LINES             ",
	"GL_LINE_LOOP         ",
	"GL_LINE_STRIP        ",
	"GL_TRIANGLES         ",
	"GL_TRIANGLE_STRIP    ",
	"GL_TRIANGLE_FAN      "
};

static IMG_CHAR * const pszDrawElementsFunctionNames[] =
{
	"DrawElements               ",
	"DrawElements (V VBO)       ",
	"DrawElements (I VBO)       ",
	"DrawElements (V and I VBO) "
};

static IMG_CHAR * const pszDrawArraysFunctionNames[] =
{
	"DrawArrays                 ",
	"DrawArrays   (V VBO)       ",
};

static IMG_CHAR* const pszEnables[GLES2_NUMBER_PROFILE_ENABLES] =
{
	"POINTSIZE  ",
	"CULLFACE   ",
	"POLYOFFSET ",
	"SCISSOR    ",
	"ALPHABLEND ",
	"MSALPHACOV ",
	"MSSAMPALPHA",
	"MSSAMPCOV  ",
	"STENCILTEST",
	"DEPTHTEST  ",
	"DITHER     "
};

#endif /* (DEBUG) || (TIMING_LEVEL > 1) */

/***********************************************************************************
 Function Name      : InitProfileData
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Resets metrics timers and finds CPU speed
************************************************************************************/
IMG_VOID IMG_INTERNAL InitProfileData(GLES2Context *gc)
{
	GLES2MemSet(gc->sDrawArraysCalls, (IMG_UINT8)0, sizeof(gc->sDrawArraysCalls));
	GLES2MemSet(gc->sDrawElementsCalls, (IMG_UINT8)0, sizeof(gc->sDrawElementsCalls));

	gc->ui32VBOMemCurrent = 0;
	gc->ui32VBOHighWaterMark = 0;
	gc->ui32VBONamesGenerated = 0;

	gc->psStateMetricData = IMG_NULL;
}

#if defined(DEBUG) || (TIMING_LEVEL > 1)

/***********************************************************************************
 Function Name      : DrawCallProfiling
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Displays primitive call/primitive type profiling data
************************************************************************************/
static IMG_VOID DrawCallProfiling(GLES2Context *gc)
{
	IMG_CHAR psString[256];
	IMG_UINT32 i, j;

	PVRSRVProfileOutput(gc->pvFileInfo, "Draw calls profiling");
	PVRSRVProfileOutput(gc->pvFileInfo, "");

	/* DrawArrays */
	for(j=PROFILE_DRAW_NO_VBO; j<=PROFILE_DRAW_VERTEX_VBO; j++)
	{
		IMG_BOOL bFound;

		bFound = IMG_FALSE;
		
		for(i=GL_POINTS; i<GL_TRIANGLE_FAN+1; i++)
		{
			if(gc->sDrawArraysCalls[j].ui32CallCount[i])
			{
				bFound = IMG_TRUE;
			}
		}

		if(bFound)
		{
			sprintf(psString, "%s   Vertices      Calls Vertices/Frame Calls/Frame", pszDrawArraysFunctionNames[j]);
			PVRSRVProfileOutput(gc->pvFileInfo, psString);

			for(i=GL_POINTS; i<GL_TRIANGLE_FAN+1; i++)
			{
				if(gc->sDrawArraysCalls[j].ui32CallCount[i])
				{
					sprintf(psString, "%s       %10u %10u     %10u  %10u", 
						pszPrimNames[i], 
						gc->sDrawArraysCalls[j].ui32VertexCount[i],
						gc->sDrawArraysCalls[j].ui32CallCount[i],
						gc->sDrawArraysCalls[j].ui32VertexCount[i] / GLES2_CALLS(GLES2_TIMER_SWAP_BUFFERS),
						gc->sDrawArraysCalls[j].ui32CallCount[i] / GLES2_CALLS(GLES2_TIMER_SWAP_BUFFERS));

					PVRSRVProfileOutput(gc->pvFileInfo, psString);
				}
			}
		}
	}


	/* DrawElements */
	for(j=PROFILE_DRAW_NO_VBO; j<=PROFILE_DRAW_INDEX_AND_VERTEX_VBO; j++)
	{
		IMG_BOOL bFound;

		bFound = IMG_FALSE;
		
		for(i=GL_POINTS; i<GL_TRIANGLE_FAN+1; i++)
		{
			if(gc->sDrawElementsCalls[j].ui32CallCount[i])
			{
				bFound = IMG_TRUE;
			}
		}

		if(bFound)
		{
			sprintf(psString, "%s   Vertices      Calls Vertices/Frame Calls/Frame", pszDrawElementsFunctionNames[j]);

			PVRSRVProfileOutput(gc->pvFileInfo, psString);

			for(i=GL_POINTS; i<GL_TRIANGLE_FAN+1; i++)
			{
				if(gc->sDrawElementsCalls[j].ui32CallCount[i])
				{
					sprintf(psString, "%s       %10u %10u     %10u  %10u",
						pszPrimNames[i], 
						gc->sDrawElementsCalls[j].ui32VertexCount[i],
						gc->sDrawElementsCalls[j].ui32CallCount[i],
						gc->sDrawElementsCalls[j].ui32VertexCount[i] / GLES2_CALLS(GLES2_TIMER_SWAP_BUFFERS),
						gc->sDrawElementsCalls[j].ui32CallCount[i] / GLES2_CALLS(GLES2_TIMER_SWAP_BUFFERS));

					PVRSRVProfileOutput(gc->pvFileInfo, psString);
				}
			}
		}
	}
}	

/***********************************************************************************
 Function Name      : AddToStateMetric
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Updates the state table (link list) with the new state
************************************************************************************/
IMG_VOID IMG_INTERNAL AddToStateMetric(GLES2Context *gc)
{
	StateMetricData *psTemp;
	StateMetricData *psNewMetric;
	IMG_BOOL bFound;

	if(!gc->psStateMetricData)
	{
		/* our list is empty, we are adding our first entry */
		gc->psStateMetricData = GLES2Malloc(gc, sizeof(StateMetricData));

		if(!gc->psStateMetricData)
		{
			PVR_DPF((PVR_DBG_ERROR, "AddToStateMetric: Could not create metric data structure"));

			return;
		}

		gc->psStateMetricData->ui32Enables = gc->ui32Enables;
		gc->psStateMetricData->ui32Count = 1;

		gc->psStateMetricData->psNext	  = IMG_NULL;
		gc->psStateMetricData->psPrevious = IMG_NULL;
	}
	else
	{
		/* first search if this state is already in our linked list */
		psTemp = gc->psStateMetricData;

		bFound = IMG_FALSE;

		while(psTemp)
		{
			IMG_UINT32 ui32Matches = 0;

			if(psTemp->ui32Enables == gc->ui32Enables)
			{
				ui32Matches++;
			}

			if(ui32Matches == 1)
			{
				bFound = IMG_TRUE;

				break;
			}

			psTemp = psTemp->psNext;
		}

		if(bFound)
		{
			/* we have found a similar state, so just increment the usage count */
			psTemp->ui32Count++;
		}
		else
		{
			/* this is a new state so we need to add it to the list */
			psTemp = gc->psStateMetricData;

			while(psTemp->psNext)
			{
				psTemp = psTemp->psNext;
			}
			
			psNewMetric = (StateMetricData*)GLES2Malloc(gc, sizeof(StateMetricData));

			if(!psNewMetric)
			{
				PVR_DPF((PVR_DBG_ERROR, "AddToStateMetric: Could not create metric data structure"));

				return;
			}

			psNewMetric->ui32Enables = gc->ui32Enables;

			psNewMetric->ui32Count = 1;

			psNewMetric->psNext = 0;
			psNewMetric->psPrevious = psTemp;

			psTemp->psNext = psNewMetric;
		}
	}
}


/***********************************************************************************
 Function Name      : PrintMetricStateRow
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Displays a row of enables in the state table (link list) 
************************************************************************************/
static IMG_VOID PrintMetricStateRow(GLES2Context *gc, IMG_CHAR *psEnableName,
							 IMG_UINT32 ui32Bit, IMG_UINT32 ui32NumberStates)
{
	IMG_UINT32 i, j;
	IMG_CHAR psBaseString[256], *psString = psBaseString;
	StateMetricData *psTemp;
	
	psString += sprintf(psString, "%s ", psEnableName);

	for(i=0; i<ui32NumberStates; i++)
	{
		psTemp = gc->psStateMetricData;

		for(j=0; j<i; j++)
		{
			psTemp = psTemp->psNext;
		}

		if(psTemp->ui32Enables & ui32Bit)
		{
			psString += sprintf(psString, " x ");
		}
		else
		{
			psString += sprintf(psString, "   ");
		}
	}

	PVRSRVProfileOutput(gc->pvFileInfo, psBaseString);
}



/***********************************************************************************
 Function Name      : DisplayStateMetricTable
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Displays the state table (link list) 
************************************************************************************/
static IMG_VOID DisplayStateMetricTable(GLES2Context *gc)
{
	IMG_CHAR psBaseString[256], *psString = psBaseString;
	IMG_UINT32 ui32CumulativeEnables = 0;
	IMG_UINT32 ui32NumberStates = 0;
	StateMetricData *psTemp;
	IMG_UINT32 i;

	psTemp = gc->psStateMetricData;

	/* First get information about all the enables in all the states, and those will
	be our head rows of the table */

	while(psTemp)
	{
		ui32CumulativeEnables |= psTemp->ui32Enables;

		psTemp = psTemp->psNext;

		ui32NumberStates++;
	}

	/* print the head of the table */

	PVRSRVProfileOutput(gc->pvFileInfo, "State combinations used in rendering\n");

	psString += sprintf(psString, "State       ");

	for(i=0; i<ui32NumberStates; i++)
	{
		psString += sprintf(psString, " %u ", i);
	}

	PVRSRVProfileOutput(gc->pvFileInfo, psBaseString);

	for(i=0; i<GLES2_NUMBER_PROFILE_ENABLES; i++)
	{
		IMG_UINT32 ui32Index = 1 << i;

		if(ui32CumulativeEnables & ui32Index)
		{
			PrintMetricStateRow(gc, pszEnables[i], ui32Index, ui32NumberStates);
		}
	}

	PVRSRVProfileOutput(gc->pvFileInfo, "");

	/* now output each state how many times has been used */
	psString = psBaseString;
	psString += sprintf(psString, "State       ");

	for(i=0; i<ui32NumberStates; i++)
	{
		psString += sprintf(psString, " %6u ", i);
	}

	PVRSRVProfileOutput(gc->pvFileInfo, psBaseString);

	psString = psBaseString;
	psString += sprintf(psString, "Usage Count ");

	psTemp = gc->psStateMetricData;

	while(psTemp)
	{
		psString += sprintf(psString, " %6u ", psTemp->ui32Count);
		psTemp = psTemp->psNext;
	}

	PVRSRVProfileOutput(gc->pvFileInfo, psBaseString);
}

/***********************************************************************************
 Function Name      : DestroyStateMetricData
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Destroys the state table (link list) 
************************************************************************************/
static IMG_VOID DestroyStateMetricData(GLES2Context *gc)
{
	StateMetricData *psTemp, *psTemp1;

	psTemp = gc->psStateMetricData;

	while(psTemp)
	{
		psTemp1 = psTemp->psNext;

		GLES2Free(IMG_NULL, psTemp);

		psTemp = psTemp1;
	}

	gc->psStateMetricData = IMG_NULL;
}


/***********************************************************************************
 Function Name      : DisplayRedundantCallInfo
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Destroys the redundant call table 
************************************************************************************/
static IMG_VOID DisplayRedundantCallInfo(GLES2Context *gc)
{
	IMG_UINT32 i;
	IMG_BOOL bInfoPresent = IMG_FALSE;
	IMG_CHAR psString[256];

	/* first see if there is any redundant information to display */
	for(i=0; i<GLES2_NUMBER_PROFILE_ENABLES; i++)
	{
		if(gc->ui32RedundantEnables[i] || gc->ui32ValidEnables[i])
		{
			bInfoPresent = IMG_TRUE;

			break;
		}
	}

	if(bInfoPresent)
	{
		PVRSRVProfileOutput(gc->pvFileInfo, "");
		PVRSRVProfileOutput(gc->pvFileInfo, "Enable/Disable Call Table");
		PVRSRVProfileOutput(gc->pvFileInfo, "");
		PVRSRVProfileOutput(gc->pvFileInfo, "State          ValidCalls RedundantCalls PerFrame-Valid-Redundant");

		for(i=0; i<GLES2_NUMBER_PROFILE_ENABLES; i++)
		{
			if(gc->ui32RedundantEnables[i] || gc->ui32ValidEnables[i])
			{
				sprintf(psString, "%s %13u %14u %14.2f %9.2f",
					pszEnables[i], 
					gc->ui32ValidEnables[i],
					gc->ui32RedundantEnables[i],
					gc->ui32ValidEnables[i]/(IMG_FLOAT)gc->asTimes[GLES2_TIMER_SWAP_BUFFERS].ui32Count,
					gc->ui32RedundantEnables[i]/(IMG_FLOAT)gc->asTimes[GLES2_TIMER_SWAP_BUFFERS].ui32Count);
				PVRSRVProfileOutput(gc->pvFileInfo, psString);
			}
		}
	}
}

#endif /* (TIMING_LEVEL > 1) */

/***********************************************************************************
 Function Name      : ProfileOutputTotals
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Displays profiling data
************************************************************************************/
IMG_VOID IMG_INTERNAL ProfileOutputTotals(GLES2Context *gc)
{
	IMG_CHAR psString[256];
	IMG_UINT32 i;
	IMG_UINT32 ui32TotalFrames, ui32ProfiledFrames;

	ui32TotalFrames = gc->asTimes[GLES2_TIMER_SWAP_BUFFERS].ui32Count & 0x7FFFFFFFL;

	if(!ui32TotalFrames)
	{
		ui32TotalFrames = gc->asTimes[GLES2_TIMER_KICK_3D].ui32Count & 0x7FFFFFFFL;
	}

	ui32ProfiledFrames = ui32TotalFrames - gc->sAppHints.ui32ProfileStartFrame;

	/* if there has been no frame do not output profile data */
	if(ui32ProfiledFrames == 0)
	{
#if defined(DEBUG) || (TIMING_LEVEL > 1)
		DestroyStateMetricData(gc);
#endif /* (TIMING_LEVEL > 1) */
	
		return;
	}

	PVRSRVInitProfileOutput(&gc->pvFileInfo);

	PVRSRVProfileOutput(gc->pvFileInfo, "");
	PVRSRVProfileOutput(gc->pvFileInfo, "Profiling (per context)");
	PVRSRVProfileOutput(gc->pvFileInfo, "=======================");
	PVRSRVProfileOutput(gc->pvFileInfo, "");

#if defined(DEBUG) || (TIMING_LEVEL > 1)

	DrawCallProfiling(gc);

	sprintf(psString, "\nVBO allocation HWM = %u", gc->ui32VBOHighWaterMark);
	PVRSRVProfileOutput(gc->pvFileInfo, psString);
	PVRSRVProfileOutput(gc->pvFileInfo, "");

	sprintf(psString, "VBO names generated = %u", gc->ui32VBONamesGenerated);
	PVRSRVProfileOutput(gc->pvFileInfo, psString);
	PVRSRVProfileOutput(gc->pvFileInfo, "");
#endif

	PVRSRVProfileOutput(gc->pvFileInfo, "");
	PVRSRVProfileOutput(gc->pvFileInfo, "Entry point profiling (Times in uSx10)");
	PVRSRVProfileOutput(gc->pvFileInfo, "");

	/* Output entry point statistics */
//	PVRSRVProfileOutput(gc->pvFileInfo, "Call                                      Number Calls  Time/Call  Calls/Frame  Time/Frame\n");
	PVRSRVProfileOutput(gc->pvFileInfo, "Function                                         Calls  Time/Call  Calls/Frame  Time/Frame");

	for(i=GLES2_TIMES_glActiveTexture; i<GLES2_NUM_TIMERS; i++)
	{
		/* at least 1 call of this type has been done */
		if(gc->asTimes[i].ui32Count)
		{
			sprintf(psString, "%s  %12u  %2.6f  %12u  %2.6f",
				pszFunctionNames[i - GLES2_TIMES_glActiveTexture],
				GLES2_CALLS(i),
				GLES2_TIME_PER_CALL(i),
				GLES2_METRIC_PER_FRAME(i, ui32ProfiledFrames),
				GLES2_TIME_PER_FRAME(i, ui32ProfiledFrames));

			PVRSRVProfileOutput(gc->pvFileInfo, psString);
		}
	}

	PVRSRVProfileOutput(gc->pvFileInfo, "");
	PVRSRVProfileOutput(gc->pvFileInfo, "");

#if defined(DEBUG) || (TIMING_LEVEL > 1)

	/* output the state table */
	DisplayStateMetricTable(gc);

	DestroyStateMetricData(gc);
	
	/* output the redundant calls information */
	DisplayRedundantCallInfo(gc);

#endif /* (TIMING_LEVEL > 1) */

	PVRSRVDeInitProfileOutput(&gc->pvFileInfo);

}

#else /* defined (TIMING) || defined (DEBUG) */


#endif /* defined (TIMING) || defined (DEBUG) */

/**************************************************************************
 End of file (profile.c)
**************************************************************************/

