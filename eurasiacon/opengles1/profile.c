/******************************************************************************
 * Name         : profile.c
 *
 * Copyright    : 2006-2008 by Imagination Technologies Limited.
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
 *****************************************************************************/

/* Exclude from non-Metrics builds */
#if defined (TIMING) || defined (DEBUG)

#include "context.h"

static const IMG_CHAR * const FunctionNames[] =
{
	"glActiveTexture          				",
	"glAlphaFunc              				",
	"glAlphaFuncx             				",
	"glBindTexture            				",
	"glBlendFunc              				",
	"glClear                  				",
	"glClearColor             				",
	"glClearColorx            				",
	"glClearDepthf            				",
	"glClearDepthx            				",
	"glClearStencil           				",
	"glClientActiveTexture    				",
	"glColor4f                				",
	"glColor4x                				",
	"glColorMask              				",
	"glColorPointer           				",
	"glCompressedTexImage2D   				",
	"glCompressedTexSubImage2D				",
	"glCopyTexImage2D         				",
	"glCopyTexSubImage2D      				",
	"glCullFace               				",
	"glDeleteTextures         				",
	"glDepthFunc              				",
	"glDepthMask              				",
	"glDepthRangef            				",
	"glDepthRangex            				",
	"glDisable                				",
	"glDisableClientState     				",
	"glDrawArrays             				",
	"glDrawElements           				",
	"glEnable                 				",
	"glEnableClientState      				",
	"glFinish                 				",
	"glFlush                  				",
	"glFogf                   				",
	"glFogfv                  				",
	"glFogx                   				",
	"glFogxv                  				",
	"glFrontFace              				",
	"glFrustumf               				",
	"glFrustumx               				",
	"glGenTextures            				",
	"glGetError               				",
	"glGetIntegerv            				",
	"glGetString              				",
	"glHint                   				",
	"glLightModelf            				",
	"glLightModelfv           				",
	"glLightModelx            				",
	"glLightModelxv           				",
	"glLightf                 				",
	"glLightfv                				",
	"glLightx                 				",
	"glLightxv                				",
	"glLineWidth              				",
	"glLineWidthx             				",
	"glLoadIdentity           				",
	"glLoadMatrixf            				",
	"glLoadMatrixx            				",
	"glLogicOp                				",
	"glMaterialf              				",
	"glMaterialfv             				",
	"glMaterialx              				",
	"glMaterialxv             				",
	"glMatrixMode             				",
	"glMultMatrixf            				",
	"glMultMatrixx            				",
	"glMultiTexCoord4f        				",
	"glMultiTexCoord4x        				",
	"glNormal3f               				",
	"glNormal3x               				",
	"glNormalPointer          				",
	"glOrthof                 				",
	"glOrthox                 				",
	"glPixelStorei            				",
	"glPointSize              				",
	"glPointSizex             				",
	"glPolygonOffset          				",
	"glPolygonOffsetx         				",
	"glPopMatrix              				",
	"glPushMatrix             				",
	"glReadPixels             				",
	"glRotatef                				",
	"glRotatex                				",
	"glSampleCoverage         				",
	"glSampleCoveragex        				",
	"glScalef                 				",
	"glScalex                 				",
	"glScissor                				",
	"glShadeModel             				",
	"glStencilFunc            				",
	"glStencilMask            				",
	"glStencilOp              				",
	"glTexCoordPointer        				",
	"glTexEnvf                				",
	"glTexEnvfv               				",
	"glTexEnvx                				",
	"glTexEnvxv               				",
	"glTexImage2D             				",
	"glTexParameterf          				",
	"glTexParameterx          				",
	"glTexSubImage2D          				",
	"glTranslatef             				",
	"glTranslatex             				",
	"glVertexPointer          				",
	"glViewport               				",

	"glClipPlanef							",
	"glClipPlanex							",

	"glQueryMatrixx							",
	"glColorMaterial						",

	"glGetTexStreamDeviceAttributeiv		",
	"glTexBindStream						",
	"glGetTexStreamDeviceName				",

	"glBindBuffer							",
	"glBufferData							",
	"glBufferSubData						",
	"glDeleteBuffers						",
	"glGenBuffers							",

	"glGetBufferParameteriv					",
	"glGetMaterialfv						",
	"glGetMaterialxv						",
	"glGetLightfv							",
	"glGetLightxv							",
	"glGetTexEnviv							",
	"glGetTexEnvfv							",
	"glGetTexEnvxv							",
	"glGetClipPlanef						",
	"glGetClipPlanex						",
	"glGetTexParameteriv					",
	"glGetTexParameterfv					",
	"glGetTexParameterxv					",
	"glGetFixedv							",
	"glGetFloatv							",
	"glGetBooleanv							",
	"glGetPointerv							",
	"glIsEnabled							",
	"glIsBuffer								",
	"glIsTexture							",

	"glPointParameterf						",
	"glPointParameterfv						",
	"glPointParameterx						",
	"glPointParameterxv						",

	"glPointSizePointerOES					",

	"glTexEnvi								",
	"glTexEnviv								",

	"glTexParameteri						",
	"glTexParameterfv						",
	"glTexParameteriv						",
	"glTexParameterxv						",

	"glColor4ub								",

	"glMatrixIndexPointerOES				",
	"glWeightPointerOES						",
	"glCurrentPaletteMatrixOES				",
	"glLoadPaletteFromModelViewMatrixOES	",

	"glDrawTexsOES							",
	"glDrawTexiOES							",
	"glDrawTexfOES							",
	"glDrawTexxOES							",
	"glDrawTexsvOES							",
	"glDrawTexivOES							",
	"glDrawTexfvOES							",
	"glDrawTexxvOES							",

	"glIsRenderbufferOES					",
	"glBindRenderbufferOES					",
	"glDeleteRenderbuffersOES				",
	"glGenRenderbuffersOES					",
	"glRenderbufferStorageOES				",
	"glGetRenderbufferParameterivOES		",
	"glGetRenderbufferStorageFormatsivOES	",
	"glIsFramebufferOES						",
	"glBindFramebufferOES					",
	"glDeleteFramebuffersOES				",
	"glGenFramebuffersOES					",
	"glCheckFramebufferStatusOES			",
	"glFramebufferTexture2DOES				",
	"glFramebufferTexture3DOES				",
	"glFramebufferRenderbufferOES			",
	"glGetFramebufferAttachmentParameterivOES",
	"glGenerateMipmapOES					",

	"glBlendFuncSeparate					",
	"glBlendEquation						",
	"glBlendEquationSeparate				",

	"glTexGeni								",
	"glTexGeniv								",
	"glTexGenf								",
	"glTexGenfv								",
	"glTexGenx								",
	"glTexGenxv								",
	"glGetTexGeniv							",
	"glGetTexGenfv							",
	"glGetTexGenxv							",

	"glMapBufferOES                         ",
	"glUnmapBufferOES                       ",
	"glGetBufferPointervOES                 ",

	"glEGLImageTargetTexture2DOES           ",
	"glEGLImageTargetRenderbufferStorageOES ",

	"glMultiDrawArrays                      ",
	"glMultiDrawElements                    "
};


#if defined (DEBUG) || (TIMING_LEVEL > 1)

static const IMG_CHAR * const PrimNames[] =
{
	"GL_POINTS        ",
	"GL_LINES         ",
	"GL_LINE_LOOP     ",
	"GL_LINE_STRIP    ",
	"GL_TRIANGLES     ",
	"GL_TRIANGLE_STRIP",
	"GL_TRIANGLE_FAN  "
};

static const IMG_CHAR * const DrawElementsFunctionNames[] =
{
	"DrawElements               ",
	"DrawElements (V VBO)       ",
	"DrawElements (I VBO)       ",
	"DrawElements (V and I VBO) "
};

static const IMG_CHAR * const DrawArraysFunctionNames[] =
{
	"DrawArrays                 ",
	"DrawArrays   (V VBO)       ",
};

static const IMG_CHAR * const RasterEnables[GLES1_NUMBER_RASTER_STATES] =
{
	"ALPHABLEND ",
	"ALPHATEST  ",
	"LOGICOP    ",
	"STENCILTEST",
	"TEXTURE0   ",
	"TEXTURE1   ",
	"TEXTURE2   ",
	"TEXTURE3   ",
	"DEPTHTEST  ",
	"POLYOFFSET ",
	"FOG        ",
	"LINESMOOTH ",
	"POINTSMOOTH",

	"CEMTEXTURE0",
	"CEMTEXTURE1",
	"CEMTEXTURE2",
	"CEMTEXTURE3",

	"GENTEXTURE0",
	"GENTEXTURE1",
	"GENTEXTURE2",
	"GENTEXTURE3",

	"TEXSTREAM0 ",
	"TEXSTREAM1 ",
	"TEXSTREAM2 ",
	"TEXSTREAM3 ",
};

static const IMG_CHAR * const FrameEnables[GLES1_NUMBER_FRAME_STATES] =
{
	"DITHER     ",
	"MULTISAMPLE",
	"SCISSOR    ",
};

static const IMG_CHAR * const TnLEnables[GLES1_NUMBER_TNL_STATES] =
{
	"LIGHT0     ",
	"LIGHT1     ",
	"LIGHT2     ",
	"LIGHT3     ",
	"LIGHT4     ",
	"LIGHT5     ",
	"LIGHT6     ",
	"LIGHT7     ",
	"LIGHTING   ",
	"RESCALE    ",
	"COLORMAT   ",
	"NORMALIZE  ",
	"CULLFACE   ",
	"CLIPPLN0   ",
	"CLIPPLN1   ",
	"CLIPPLN2   ",
	"CLIPPLN3   ",
	"CLIPPLN4   ",
	"CLIPPLN5   ",
	"PNTSPRITE  ",
	"MTXPALETTE ",
};

static const IMG_CHAR * const IgnoredEnables[GLES1_NUMBER_IGNORE_STATES] =
{
	"MSALPHACOV ",
	"MSSAMPALPHA",
	"MSSAMPCOV  ",
};

#endif /* (DEBUG) || (TIMING_LEVEL > 1) */


/***********************************************************************************
 Function Name      : InitProfileData
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Resets metrics timers and finds CPU speed
************************************************************************************/
IMG_INTERNAL IMG_VOID InitProfileData(GLES1Context *gc)
{
	GLES1MemSet(gc->sDrawArraysCalls, (IMG_UINT8)0, sizeof(gc->sDrawArraysCalls));
	GLES1MemSet(gc->sDrawElementsCalls, (IMG_UINT8)0, sizeof(gc->sDrawElementsCalls));

	gc->ui32VBOMemCurrent = 0;
	gc->ui32VBOHighWaterMark = 0;
	gc->ui32VBONamesGenerated = 0;

	gc->psStateMetricData = IMG_NULL;
}


#if defined(DEBUG) || (TIMING_LEVEL > 1)


/***********************************************************************************
 Function Name      : DrawCallProfiling
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID DrawCallProfiling(GLES1Context *gc)
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
			sprintf(psString, "%s   Vertices      Calls Vertices/Frame Calls/Frame", DrawArraysFunctionNames[j]);
			PVRSRVProfileOutput(gc->pvFileInfo, psString);

			for(i=GL_POINTS; i<GL_TRIANGLE_FAN+1; i++)
			{
				if(gc->sDrawArraysCalls[j].ui32CallCount[i])
				{
					sprintf(psString, "%s           %10u %10u     %10u  %10u",
						PrimNames[i], 
						gc->sDrawArraysCalls[j].ui32VertexCount[i],
						gc->sDrawArraysCalls[j].ui32CallCount[i],
						gc->sDrawArraysCalls[j].ui32VertexCount[i] / GLES1_CALLS(GLES1_TIMER_SWAP_BUFFERS_COUNT),
						gc->sDrawArraysCalls[j].ui32CallCount[i] / GLES1_CALLS(GLES1_TIMER_SWAP_BUFFERS_COUNT));

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
			sprintf(psString, "%s   Vertices      Calls Vertices/Frame Calls/Frame", DrawElementsFunctionNames[j]);

			PVRSRVProfileOutput(gc->pvFileInfo, psString);

			for(i=GL_POINTS; i<GL_TRIANGLE_FAN+1; i++)
			{
				if(gc->sDrawElementsCalls[j].ui32CallCount[i])
				{
					sprintf(psString, "%s           %10u %10u     %10u  %10u",
						PrimNames[i], 
						gc->sDrawElementsCalls[j].ui32VertexCount[i],
						gc->sDrawElementsCalls[j].ui32CallCount[i],
						gc->sDrawElementsCalls[j].ui32VertexCount[i] / GLES1_CALLS(GLES1_TIMER_SWAP_BUFFERS_COUNT),
						gc->sDrawElementsCalls[j].ui32CallCount[i] / GLES1_CALLS(GLES1_TIMER_SWAP_BUFFERS_COUNT));

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
IMG_INTERNAL IMG_VOID AddToStateMetric(GLES1Context *gc)
{
	StateMetricData *psTemp;
	StateMetricData *psNewMetric;
	IMG_BOOL bFound;

	if(!gc->psStateMetricData)
	{
		/* our list is empty, we are adding our first entry */
		gc->psStateMetricData = GLES1Malloc(gc, sizeof(StateMetricData));

		if(!gc->psStateMetricData)
		{
			PVR_DPF((PVR_DBG_ERROR, "AddToStateMetric: Could not create metricdata structure, possibly out of memory"));
			return;
		}

		gc->psStateMetricData->ui32RasterEnables	= gc->ui32RasterEnables;
		gc->psStateMetricData->ui32TnLEnables		= gc->ui32TnLEnables;
		gc->psStateMetricData->ui32IgnoredEnables	= gc->ui32IgnoredEnables;
		gc->psStateMetricData->ui32FrameEnables		= gc->ui32FrameEnables;

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

			if(psTemp->ui32RasterEnables == gc->ui32RasterEnables)
			{
				ui32Matches++;
			}

			if(psTemp->ui32TnLEnables == gc->ui32TnLEnables)
			{
				ui32Matches++;
			}

			if(psTemp->ui32IgnoredEnables == gc->ui32IgnoredEnables)
			{
				ui32Matches++;
			}

			if(psTemp->ui32FrameEnables == gc->ui32FrameEnables)
			{
				ui32Matches++;
			}

			if(ui32Matches == 4)
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
			
			psNewMetric = (StateMetricData*)GLES1Malloc(gc, sizeof(StateMetricData));

			if(!psNewMetric)
			{
				PVR_DPF((PVR_DBG_ERROR, "AddToStateMetric: Could not create metricdata structure, possibly out of memory"));

				return;
			}

			psNewMetric->ui32RasterEnables = gc->ui32RasterEnables;
			psNewMetric->ui32TnLEnables    = gc->ui32TnLEnables;
			psNewMetric->ui32IgnoredEnables = gc->ui32IgnoredEnables;
			psNewMetric->ui32FrameEnables  = gc->ui32FrameEnables;

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
static IMG_VOID PrintMetricStateRow(GLES1Context *gc, const IMG_CHAR *psEnableName, IMG_UINT32 ui32Which,
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

		switch(ui32Which)
		{
			case 0:
			{
				if(psTemp->ui32RasterEnables & ui32Bit)
				{
					psString += sprintf(psString, " x ");
				}
				else
				{
					psString += sprintf(psString, "   ");
				}

				break;
			}
			case 1:
			{
				if(psTemp->ui32TnLEnables & ui32Bit)
				{
					psString += sprintf(psString, " x ");
				}
				else
				{
					psString += sprintf(psString, "   ");
				}

				break;
			}
			case 2:
			{
				if(psTemp->ui32IgnoredEnables & ui32Bit)
				{
					psString += sprintf(psString, " x ");
				}
				else
				{
					psString += sprintf(psString, "   ");
				}

				break;
			}
			case 3:
			{
				if(psTemp->ui32FrameEnables & ui32Bit)
				{
					psString += sprintf(psString, " x ");
				}
				else
				{
					psString += sprintf(psString, "   ");
				}

				break;
			}
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
static IMG_VOID DisplayStateMetricTable(GLES1Context *gc)
{
	IMG_CHAR psBaseString[256], *psString = psBaseString;
	IMG_UINT32 ui32CumulativeRasterEnables = 0;
	IMG_UINT32 ui32CumulativeTnLEnables = 0;
	IMG_UINT32 ui32CumulativeIgnoredEnables = 0;
	IMG_UINT32 ui32CumulativeFrameEnables = 0;
	StateMetricData *psTemp;
	IMG_UINT32 i;
	IMG_UINT32 ui32NumberStates = 0;

	psTemp = gc->psStateMetricData;

	/* First get information about all the enables in all the states, and those will
	be our head rows of the table */

	while(psTemp)
	{
		ui32CumulativeRasterEnables |= psTemp->ui32RasterEnables;
		ui32CumulativeTnLEnables	|= psTemp->ui32TnLEnables;
		ui32CumulativeIgnoredEnables|= psTemp->ui32IgnoredEnables;
		ui32CumulativeFrameEnables	|= psTemp->ui32FrameEnables;

		psTemp = psTemp->psNext;

		ui32NumberStates++;
	}

	/* print the head of the table */

	PVRSRVProfileOutput(gc->pvFileInfo, "State combinations used in rendering");
	PVRSRVProfileOutput(gc->pvFileInfo, "");

	psString = psBaseString;
	psString += sprintf(psString, "State       ");

	for(i=0; i<ui32NumberStates; i++)
	{
		psString += sprintf(psString, " %u ", i);
	}

	PVRSRVProfileOutput(gc->pvFileInfo, psBaseString);

	/* start with raster enables */
	for(i=0; i<GLES1_NUMBER_RASTER_STATES; i++)
	{
		IMG_UINT32 ui32Index = 1 << i;

		if(ui32CumulativeRasterEnables & ui32Index)
		{
			PrintMetricStateRow(gc, RasterEnables[i], 0, ui32Index, ui32NumberStates);
		}
	}

	/* display the TnL enables */
	for(i=0; i<GLES1_NUMBER_TNL_STATES; i++)
	{
		IMG_UINT32 ui32Index = 1 << i;

		if(ui32CumulativeTnLEnables & ui32Index)
		{
			PrintMetricStateRow(gc, TnLEnables[i], 1, ui32Index, ui32NumberStates);
		}
	}

	/* display the full screen enables */
	for(i=0; i<GLES1_NUMBER_FRAME_STATES; i++)
	{
		IMG_UINT32 ui32Index = 1 << i;

		if(ui32CumulativeFrameEnables & ui32Index)
		{
			PrintMetricStateRow(gc, FrameEnables[i], 3, ui32Index, ui32NumberStates);
		}
	}

	/* display the Ignored enables */
	for(i=0; i<GLES1_NUMBER_IGNORE_STATES; i++)
	{
		IMG_UINT32 ui32Index = 1 << i;

		if(ui32CumulativeIgnoredEnables & ui32Index)
		{
			PrintMetricStateRow(gc, IgnoredEnables[i], 2, ui32Index, ui32NumberStates);
		}
	}

	PVRSRVProfileOutput(gc->pvFileInfo, "");

	/* now output how many times each state has been used */

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

		PVRSRVProfileOutput(gc->pvFileInfo, psBaseString);

		psTemp = psTemp->psNext;
	}

}


/***********************************************************************************
 Function Name      : DestroyStateMetricData
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Destroys the state table (link list) 
************************************************************************************/
static IMG_VOID DestroyStateMetricData(GLES1Context *gc)
{
	StateMetricData *psTemp, *psTemp1;

	psTemp = gc->psStateMetricData;

	while(psTemp)
	{
		psTemp1 = psTemp->psNext;

		GLES1Free(IMG_NULL, psTemp);

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
static IMG_VOID DisplayRedundantCallInfo(GLES1Context *gc)
{
	IMG_UINT32 i;
	IMG_BOOL bInfoPresent = IMG_FALSE;
	IMG_CHAR psString[256];

	/* first see if there is any redundant information to display */
	for(i=0; i<GLES1_NUMBER_RASTER_STATES; i++)
	{
		if(gc->ui32RedundantStatesRaster[i] || gc->ui32ValidStatesRaster[i])
		{
			bInfoPresent = IMG_TRUE;
			break;
		}
	}

	for(i=0; i<GLES1_NUMBER_TNL_STATES; i++)
	{
		if(gc->ui32RedundantStatesTnL[i] || gc->ui32ValidStatesTnL[i])
		{
			bInfoPresent = IMG_TRUE;
			break;
		}
	}

	for(i=0; i<GLES1_NUMBER_FRAME_STATES; i++)
	{
		if(gc->ui32RedundantStatesFrame[i] || gc->ui32ValidStatesFrame[i])
		{
			bInfoPresent = IMG_TRUE;
			break;
		}
	}

	for(i=0; i<GLES1_NUMBER_IGNORE_STATES; i++)
	{
		if(gc->ui32RedundantStatesIgnored[i] || gc->ui32ValidStatesIgnored[i])
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

		for(i=0; i<GLES1_NUMBER_RASTER_STATES; i++)
		{
			if(gc->ui32RedundantStatesRaster[i] || gc->ui32ValidStatesRaster[i])
			{
				sprintf(psString, "%s %13u %14u %14.2f %9.2f",
					RasterEnables[i], 
					gc->ui32ValidStatesRaster[i],
					gc->ui32RedundantStatesRaster[i],
					gc->ui32ValidStatesRaster[i]/(IMG_FLOAT)gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT].ui32Count,
					gc->ui32RedundantStatesRaster[i]/(IMG_FLOAT)gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT].ui32Count);
				PVRSRVProfileOutput(gc->pvFileInfo, psString);
			}
		}

		for(i=0; i<GLES1_NUMBER_TNL_STATES; i++)
		{
			if(gc->ui32RedundantStatesTnL[i] || gc->ui32ValidStatesTnL[i])
			{
				sprintf(psString, "%s %13u %14u %14.2f %9.2f",
					TnLEnables[i], 
					gc->ui32ValidStatesTnL[i],
					gc->ui32RedundantStatesTnL[i],
					gc->ui32ValidStatesTnL[i]/(IMG_FLOAT)gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT].ui32Count,
					gc->ui32RedundantStatesTnL[i]/(IMG_FLOAT)gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT].ui32Count);
				PVRSRVProfileOutput(gc->pvFileInfo, psString);
			}
		}

		for(i=0; i<GLES1_NUMBER_FRAME_STATES; i++)
		{
			if(gc->ui32RedundantStatesFrame[i] || gc->ui32ValidStatesFrame[i])
			{
				sprintf(psString, "%s %13u %14u %14.2f %9.2f",
					FrameEnables[i], 
					gc->ui32ValidStatesFrame[i],
					gc->ui32RedundantStatesFrame[i],
					gc->ui32ValidStatesFrame[i]/(IMG_FLOAT)gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT].ui32Count,
					gc->ui32RedundantStatesFrame[i]/(IMG_FLOAT)gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT].ui32Count);
				PVRSRVProfileOutput(gc->pvFileInfo, psString);
			}
		}

		for(i=0; i<GLES1_NUMBER_IGNORE_STATES; i++)
		{
			if(gc->ui32RedundantStatesIgnored[i] || gc->ui32ValidStatesIgnored[i])
			{
				sprintf(psString, "%s %13u %14u %14.2f %9.2f",
					IgnoredEnables[i], 
					gc->ui32ValidStatesIgnored[i],
					gc->ui32RedundantStatesIgnored[i],
					gc->ui32ValidStatesIgnored[i]/(IMG_FLOAT)gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT].ui32Count,
					gc->ui32RedundantStatesIgnored[i]/(IMG_FLOAT)gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT].ui32Count);
				PVRSRVProfileOutput(gc->pvFileInfo, psString);
			}
		}
	}
}

#endif /* (TIMING_LEVEL > 1) */



/***********************************************************************************
 Function Name      : ProfileOutputTotals
 Inputs             : gc, ui32Frames
 Outputs            : -
 Returns            : -
 Description        : Displays profiling data
************************************************************************************/
IMG_INTERNAL IMG_VOID ProfileOutputTotals(GLES1Context *gc)
{
	IMG_CHAR psString[256];
	IMG_UINT32 i, ui32Frames;

	ui32Frames = gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT].ui32Count & 0x7FFFFFFFL;

	if(!ui32Frames)
	{
		ui32Frames = gc->asTimes[GLES1_TIMER_KICK_3D].ui32Count & 0x7FFFFFFFL;
	}

	/* if there has been no frame do not output profile data */
	if(ui32Frames == 0)
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
	PVRSRVProfileOutput(gc->pvFileInfo, "Call                                                      Number Calls  Time/Call  Calls/Frame  Time/Frame");

	for(i=GLES1_TIMES_glActiveTexture; i<GLES1_TIMES_glGetBufferPointervOES+1; i++)
	{
		/* at least 1 call of this type has been done */
		if(gc->asTimes[i].ui32Count)
		{
			sprintf(psString, "%s  %12u  %2.6f  %12u  %2.6f",
				FunctionNames[i - GLES1_TIMES_glActiveTexture],
				GLES1_CALLS(i),
				GLES1_TIME_PER_CALL(i),
				GLES1_METRIC_PER_FRAME(i),
				GLES1_TIME_PER_FRAME(i));

			PVRSRVProfileOutput(gc->pvFileInfo, psString);
		}
	}

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

#else /* defined(TIMING) || defined(DEBUG) */


#endif /* defined (TIMING) || defined (DEBUG) */

/******************************************************************************
 End of file (profile.c)
******************************************************************************/

