/******************************************************************************
 * Name         : metrics.c
 *
 * Copyright    : 2006-2007 by Imagination Technologies Limited.
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
 * $Log: metrics.c $
 *
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#if defined(TIMING) || defined(DEBUG)

#include "context.h"

/***********************************************************************************
 Function Name      : GetFrameTime
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Gets total time for a frame
************************************************************************************/
IMG_INTERNAL IMG_VOID GetFrameTime(GLES1Context *gc)
{

	IMG_UINT32 ui32TimeNow = PVRSRVMetricsTimeNow();

	IMG_UINT32 ui32PreviousFrameTime = gc->ui32PrevTime;
	IMG_FLOAT fTimeInFrame = (ui32TimeNow - ui32PreviousFrameTime) * gc->fCPUSpeed;

	/* Don't include first frame time or non-profiled times */
	if(ui32PreviousFrameTime && (GLES1_CHECK_BETWEEN_START_END_FRAME))
	{
		gc->asTimes[GLES1_TIMER_TOTAL_FRAME_TIME].fStack += fTimeInFrame;
	}

	gc->ui32PrevTime = ui32TimeNow;
}

#if defined(TIMING)
IMG_INTERNAL IMG_VOID StartUpMemSpeedTest(GLES1Context *gc)
{
	IMG_UINT32 ui32BlockSize, ui32MaxSize, ui32BlockNumber;
	IMG_UINT32 *pui32VBuffer, *pui32HostBuffer1, *pui32HostBuffer2;
	IMG_UINT32 *pui32RealHostBuffer1, *pui32RealHostBuffer2;
	IMG_CHAR szTempBuffer[1024];
	IMG_UINT32 ui32BufLen;

	PVR_DPF((PVR_DBG_WARNING, "Running StartUpMemSpeedTest. High memory watermark will be ruined. Disable using apphint EnableMemorySpeedTest"));

	ui32MaxSize = 128*1024;

	if(ui32MaxSize > (gc->apsBuffers[CBUF_TYPE_VERTEX_DATA_BUFFER]->ui32BufferLimitInBytes << 2))
	{
		PVR_DPF((PVR_DBG_ERROR,"StartUpMemSpeedTest: VB is too small - aborting test"));
		return;
	}

	pui32VBuffer = gc->apsBuffers[CBUF_TYPE_VERTEX_DATA_BUFFER]->pui32BufferBase;

	pui32RealHostBuffer1 = (IMG_UINT32 *)GLES1Malloc(gc, 1024*1024+32);

	if (!pui32RealHostBuffer1)
	{
		PVR_DPF((PVR_DBG_ERROR,"StartUpMemSpeedTest: Failed host alloc 1 - aborting test"));
		return;
	}

	pui32RealHostBuffer2 = 	(IMG_UINT32 *)GLES1Malloc(gc, 1024*1024+32);

	if (!pui32RealHostBuffer2)
	{
		GLES1Free(gc, pui32RealHostBuffer1);
		PVR_DPF((PVR_DBG_ERROR,"StartUpMemSpeedTest: Failed host alloc 2 - aborting test"));
		return;
	}

	/* Align the addresses */
	pui32HostBuffer1 = (IMG_UINT32 *)(((unsigned int)pui32RealHostBuffer1 & 0xFFFFFFE0UL) + 32);
	pui32HostBuffer2 = (IMG_UINT32 *)(((unsigned int)pui32RealHostBuffer2 & 0xFFFFFFE0UL) + 32);


	/* Spacer */
	ui32BufLen = sprintf(szTempBuffer, "Bytes  :");

	/* Draw column headers */
	for (ui32BlockSize=512; ui32BlockSize<=ui32MaxSize; ui32BlockSize<<=1)
	{
		ui32BufLen += sprintf(szTempBuffer + ui32BufLen, "%7u ",ui32BlockSize);
	}

	PVR_TRACE((szTempBuffer));

	/* Spacer */
	ui32BufLen = sprintf(szTempBuffer, "--------");

	/* Draw separator */
	for (ui32BlockSize=512; ui32BlockSize<=ui32MaxSize; ui32BlockSize<<=1)
	{
		ui32BufLen += sprintf(szTempBuffer + ui32BufLen, "-------|");
	}

	PVR_TRACE((szTempBuffer));

	/* Row header */
	ui32BufLen = sprintf(szTempBuffer, "H -> VB:");

	/* Host to VB copy */
	for (ui32BlockSize=512,ui32BlockNumber=0; ui32BlockSize<=ui32MaxSize; ui32BlockSize<<=1,ui32BlockNumber++)
	{
		IMG_UINT32 ui32Start, ui32Stop, ui32Total;
		IMG_UINT32 ui32Count;
		IMG_FLOAT fMB, fSeconds;

		ui32Start = PVRSRVMetricsTimeNow();

		for (ui32Count=0; ui32Count<10; ui32Count++)
		{
			GLES1MemCopy(pui32VBuffer, pui32HostBuffer1, ui32BlockSize);
		}

		ui32Stop = PVRSRVMetricsTimeNow();

		ui32Total = ui32Stop - ui32Start;

		fMB = 10.0f*ui32BlockSize;
		fMB /= (1024.0f*1024.0f);

		fSeconds = (IMG_FLOAT)ui32Total;
		fSeconds *= (gc->fCPUSpeed/1000.0f);

		ui32BufLen += sprintf(szTempBuffer + ui32BufLen, "%7.2f ", fSeconds?fMB/fSeconds:0.0f);
	}

	PVR_TRACE((szTempBuffer));

	/* Row header */
	ui32BufLen = sprintf(szTempBuffer, "H ->  H:");

	/* Host to Host copy */
	for (ui32BlockSize=512,ui32BlockNumber=0; ui32BlockSize<=ui32MaxSize; ui32BlockSize<<=1,ui32BlockNumber++)
	{
		IMG_UINT32 ui32Start, ui32Stop, ui32Total;
		IMG_UINT32 ui32Count;
		IMG_FLOAT fMB, fSeconds;

		ui32Start = PVRSRVMetricsTimeNow();

		for (ui32Count=0; ui32Count<10; ui32Count++)
		{
			GLES1MemCopy(pui32HostBuffer2, pui32HostBuffer1, ui32BlockSize);
		}

		ui32Stop = PVRSRVMetricsTimeNow();

		ui32Total = ui32Stop - ui32Start;

		fMB = 10.0f*ui32BlockSize;
		fMB /= (1024.0f*1024.0f);

		fSeconds = (IMG_FLOAT)ui32Total;
		fSeconds *= (gc->fCPUSpeed/1000.0f);

		ui32BufLen += sprintf(szTempBuffer + ui32BufLen, "%7.2f ", fSeconds?fMB/fSeconds:0.0f);
	}

	PVR_TRACE((szTempBuffer));

	GLES1Free(gc, pui32RealHostBuffer1);
	GLES1Free(gc, pui32RealHostBuffer2);
}

#endif /* #if defined(TIMING) */
/***********************************************************************************
 Function Name      : InitMetrics
 Inputs             : gc
 Outputs            : -
 Returns            : Success
 Description        : Initialises the metrics data for a context
************************************************************************************/
IMG_INTERNAL IMG_BOOL InitMetrics(GLES1Context *gc)
{

	GLES1MemSet(gc->asTimes, 0, sizeof(PVR_Temporal_Data)*GLES1_NUM_TIMERS);

	if(!gc->fCPUSpeed)
	{
		gc->fCPUSpeed = PVRSRVMetricsGetCPUFreq();

		if(!gc->fCPUSpeed)
		{
			PVR_DPF((PVR_DBG_ERROR,"InitMetrics: Failed to get CPU freq"));

			return IMG_FALSE;
		}
		else
		{
			gc->fCPUSpeed = 1000.0f/gc->fCPUSpeed;
		}
	}
#if defined(TIMING)
	/* The memory speed tests consumes 2MB of memory and thus ruins the high memory watermark */
	if(gc->sAppHints.bEnableMemorySpeedTest)
	{
		StartUpMemSpeedTest(gc);
	}
#endif
	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : OutputMetrics
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Outputs the metrics data for a context 
************************************************************************************/
IMG_INTERNAL IMG_VOID OutputMetrics(GLES1Context *gc)
{
	IMG_UINT32  ui32TotalFrames, ui32Frames, ui32Loop;

	ui32TotalFrames = gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_COUNT].ui32Count & 0x7FFFFFFFL;

	if (gc->sAppHints.bDisableMetricsOutput)
	{
		return;
	}

	for(ui32Loop=0; ui32Loop < GLES1_NUM_TIMERS; ui32Loop++)
	{
		if(gc->asTimes[ui32Loop].ui32Count & 0x80000000L)
		{
			PVR_TRACE(("Warning: Timer %d in ON position", ui32Loop));
			gc->asTimes[ui32Loop].ui32Count &= 0x7FFFFFFFL;
		}
	}

	if(!ui32TotalFrames)
	{
		ui32TotalFrames = gc->asTimes[GLES1_TIMER_KICK_3D].ui32Count & 0x7FFFFFFFL;
	}

	/* Dump out the data */
	if(ui32TotalFrames)
	{
		IMG_FLOAT fCopyImgCall, fCopyImgFrame;
		IMG_FLOAT fCopySubImgCall, fCopySubImgFrame;
		IMG_FLOAT fTexAllocateCall, fTexAllocateFrame;
		IMG_FLOAT fTexTranslateCall, fTexTranslateFrame;
		IMG_FLOAT fTexLoadGhostCall, fTexLoadGhostFrame;
		IMG_FLOAT fTexReadbackCall, fTexReadbackFrame;
		IMG_FLOAT fLoadCall, fLoadFrame, fLoadRate;
		IMG_FLOAT fSubLoadCall, fSubLoadFrame, fSubLoadRate;
		IMG_FLOAT fCompLoadCall, fCompLoadFrame, fCompLoadRate;
		IMG_FLOAT fCompSubLoadCall, fCompSubLoadFrame, fCompSubLoadRate;
		IMG_FLOAT fTotalDriverTime, fTotalDrawTime;
		IMG_UINT32 ui32TotalDrawCalls;
		IMG_FLOAT fkB;

		fCopyImgCall		= 0.0f;
		fCopyImgFrame		= 0.0f;
		fCopySubImgCall     = 0.0f;
		fCopySubImgFrame    = 0.0f;
		fTexAllocateCall	= 0.0f;
		fTexAllocateFrame	= 0.0f;
		fTexTranslateCall	= 0.0f;
		fTexTranslateFrame	= 0.0f;
		fTexLoadGhostCall	= 0.0f;
		fTexLoadGhostFrame	= 0.0f;
		fTexReadbackCall	= 0.0f;
		fTexReadbackFrame	= 0.0f;
		fLoadCall			= 0.0f;
		fLoadFrame			= 0.0f;
		fLoadRate			= 0.0f;
		fSubLoadCall		= 0.0f;
		fSubLoadFrame		= 0.0f;
		fSubLoadRate		= 0.0f;
		fCompLoadCall		= 0.0f;
		fCompLoadFrame		= 0.0f;
		fCompLoadRate		= 0.0f;
		fCompSubLoadCall	= 0.0f;
		fCompSubLoadFrame	= 0.0f;
		fCompSubLoadRate	= 0.0f;
		fTotalDriverTime	= 0.0f;
		fTotalDrawTime		= 0.0f;

		fTotalDriverTime = 0;

		for(ui32Loop=GLES1_TIMES_glActiveTexture; ui32Loop<=(GLES1_NUM_TIMERS-1); ui32Loop++)
		{
			fTotalDriverTime += gc->asTimes[ui32Loop].ui32Total*gc->fCPUSpeed;
		}

		ui32TotalDrawCalls = gc->asTimes[GLES1_TIMES_glDrawArrays].ui32Count +
							 gc->asTimes[GLES1_TIMES_glDrawElements].ui32Count;

		fTotalDrawTime = (gc->asTimes[GLES1_TIMES_glDrawArrays].ui32Total +
						  gc->asTimes[GLES1_TIMES_glDrawElements].ui32Total)*gc->fCPUSpeed; 

		ui32Frames = MIN(ui32TotalFrames, gc->sAppHints.ui32ProfileEndFrame);
		ui32Frames = MIN(ui32TotalFrames, ui32Frames - gc->sAppHints.ui32ProfileStartFrame + 1);
		
		if(!ui32Frames)
		{
			PVR_TRACE(("Profiled frames: 0 --- no metrics to display"));

			return;
		}

		PVR_TRACE((" Total Frames                                  %10d", ui32TotalFrames));
		PVR_TRACE((" Profiled Frames                         (%d-%d) %10d", gc->sAppHints.ui32ProfileStartFrame, MIN(ui32TotalFrames, gc->sAppHints.ui32ProfileEndFrame), ui32Frames));
		PVR_TRACE((" Average Elapsed Time per Profiled Frame (ms)  %10.4f", gc->asTimes[GLES1_TIMER_TOTAL_FRAME_TIME].fStack / ui32Frames));
		PVR_TRACE((" Average Driver Time per Profiled Frame (ms)   %10.4f", fTotalDriverTime / ui32Frames));

		if(gc->asTimes[GLES1_TIMER_TOTAL_FRAME_TIME].fStack)
		{
			PVR_TRACE((" Estimated FPS                                   %10.4f", 1000.0f*ui32Frames/gc->asTimes[GLES1_TIMER_TOTAL_FRAME_TIME].fStack));
		}

		PVR_TRACE((" "));

		PVR_TRACE(("     Statistics per profiled frame          [Calls/Time(ms)]"));
		PVR_TRACE((" Total Prepare to draw                  %10d/%10.4f", gc->asTimes[GLES1_TIMER_PREPARE_TO_DRAW_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_PREPARE_TO_DRAW_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE((" Total SwapBuffers                      %10d/%10.4f", gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SWAP_BUFFERS_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE((" Total SGXKickTA                        %10d/%10.4f", gc->asTimes[GLES1_TIMER_SGXKICKTA_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SGXKICKTA_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));

		for(ui32Loop=0; ui32Loop<CBUF_NUM_BUFFERS ;ui32Loop++)
		{
			if(gc->apsBuffers[ui32Loop] && gc->apsBuffers[ui32Loop]->ui32KickCount)
			{
				PVR_TRACE(("   %s kick limit:     %10d/       -", pszBufferNames[ui32Loop], gc->apsBuffers[ui32Loop]->ui32KickCount/ui32Frames));
			}
		}

		PVR_TRACE((" Total Renders                          %10.4f/       -", (IMG_FLOAT)gc->asTimes[GLES1_TIMER_KICK_3D].ui32Count/ui32Frames));
		PVR_TRACE((" Total Wait for 3D                      %10d/%10.4f", gc->asTimes[GLES1_TIMER_WAITING_FOR_3D_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_WAITING_FOR_3D_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
        PVR_TRACE((" Total Wait for TA                      %10d/%10.4f", gc->asTimes[GLES1_TIMER_WAITING_FOR_TA_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_WAITING_FOR_TA_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE((" Total ValidateState()                  %10d/%10.4f", gc->asTimes[GLES1_TIMER_VALIDATE_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_VALIDATE_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - Setup_VAO_AttribStreams()           %10d/%10.4f", gc->asTimes[GLES1_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - Setup_VAO_AttribPointers()          %10d/%10.4f", gc->asTimes[GLES1_TIMER_SETUP_VAO_ATTRIB_POINTERS_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SETUP_VAO_ATTRIB_POINTERS_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - Setup_TextureState()                %10d/%10.4f", gc->asTimes[GLES1_TIMER_SETUP_TEXTURESTATE_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SETUP_TEXTURESTATE_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - Setup_Streams()                     %10d/%10.4f", gc->asTimes[GLES1_TIMER_SETUP_STREAMS_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SETUP_STREAMS_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - UpdateMVP()                         %10d/%10.4f", gc->asTimes[GLES1_TIMER_UPDATEMVP_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_UPDATEMVP_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - Setup_VertexShader()                %10d/%10.4f", gc->asTimes[GLES1_TIMER_SETUP_VERTEXSHADER_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SETUP_VERTEXSHADER_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - Setup_VertexConstants()             %10d/%10.4f", gc->asTimes[GLES1_TIMER_SETUP_VERTEXCONSTANTS_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SETUP_VERTEXCONSTANTS_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - Setup_Outputs()                     %10d/%10.4f", gc->asTimes[GLES1_TIMER_SETUP_OUTPUTS_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SETUP_OUTPUTS_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - Setup_VertexVariant()               %10d/%10.4f", gc->asTimes[GLES1_TIMER_SETUP_VERTEXVARIANT_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SETUP_VERTEXVARIANT_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - Setup_RenderState()                 %10d/%10.4f", gc->asTimes[GLES1_TIMER_SETUP_RENDERSTATE_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SETUP_RENDERSTATE_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - Setup_FragmentShader()              %10d/%10.4f", gc->asTimes[GLES1_TIMER_SETUP_FRAGMENTSHADER_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SETUP_FRAGMENTSHADER_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));

		PVR_TRACE((" Total EmitState()                      %10d/%10.4f", gc->asTimes[GLES1_TIMER_STATE_EMIT_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_STATE_EMIT_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));

		PVR_TRACE(("  - WritePDSPixelShaderProgram()        %10d/%10.4f", gc->asTimes[GLES1_TIMER_WRITEPDSPIXELSHADERPROGRAM_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_WRITEPDSPIXELSHADERPROGRAM_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		
		PVR_TRACE(("  - WritePDSVertexShaderProgramWithVAO()    %10d/%10.4f", gc->asTimes[GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - WritePDSVertexShaderProgramWithVAO() - Generate or Patch Program    %10d/%10.4f", gc->asTimes[GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_GENERATE_OR_PATCH_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_GENERATE_OR_PATCH_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));

		if(gc->sAppHints.bEnableStaticPDSVertex)
		{	
			PVR_TRACE(("  - WritePDSStaticVertexShader()        %10d/%10.4f", gc->asTimes[GLES1_TIMER_WRITEPDSVERTEXSHADERPROGRAM_TIME].ui32Count/ui32Frames, (gc->asTimes[GLES1_TIMER_WRITEPDSVERTEXSHADERPROGRAM_TIME].ui32Total)*gc->fCPUSpeed/ui32Frames));
			PVR_TRACE(("  - WriteAuxiliaryPDSVertexShader()     %10d/%10.4f", gc->asTimes[GLES1_TIMER_WRITEPDSVERTEXAUXILIARY_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_WRITEPDSVERTEXAUXILIARY_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		
		}
		else
		{
			PVR_TRACE(("  - WritePDSVertexShaderProgram()       %10d/%10.4f", gc->asTimes[GLES1_TIMER_WRITEPDSVERTEXSHADERPROGRAM_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_WRITEPDSVERTEXSHADERPROGRAM_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		}

		PVR_TRACE(("  - WritePDSUSEShaderSAProgram()        %10d/%10.4f", gc->asTimes[GLES1_TIMER_WRITEPDSUSESHADERSAPROGRAM_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_WRITEPDSUSESHADERSAPROGRAM_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - WriteMTEState()                     %10d/%10.4f", gc->asTimes[GLES1_TIMER_WRITEMTESTATE_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_WRITEMTESTATE_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - WriteMTEStateCopyPrograms()         %10d/%10.4f", gc->asTimes[GLES1_TIMER_WRITEMTESTATECOPYPROGRAMS_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_WRITEMTESTATECOPYPROGRAMS_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - SetupStateUpdate()                  %10d/%10.4f", gc->asTimes[GLES1_TIMER_SETUPSTATEUPDATE_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_SETUPSTATEUPDATE_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE(("  - WriteVDMControlStream()             %10d/%10.4f", gc->asTimes[GLES1_TIMER_WRITEVDMCONTROLSTREAM_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_WRITEVDMCONTROLSTREAM_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));

		if(gc->asTimes[GLES1_TIMER_EMIT_BUGFIX_TIME].ui32Count)
		{
			PVR_TRACE(("  - BugFix()                            %10d/%10.4f", gc->asTimes[GLES1_TIMER_EMIT_BUGFIX_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_EMIT_BUGFIX_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		}	

		PVR_TRACE((" USE Codeheap Alloc - Fragment          %10d/%10.4f", gc->asTimes[GLES1_TIMER_USECODEHEAP_FRAG_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_USECODEHEAP_FRAG_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE((" USE Codeheap Alloc - Vertex            %10d/%10.4f", gc->asTimes[GLES1_TIMER_USECODEHEAP_VERT_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_USECODEHEAP_VERT_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));

		PVR_TRACE((" Total Primitive Draw                   %10d/%10.4f", ui32TotalDrawCalls/ui32Frames, fTotalDrawTime/ui32Frames));
		PVR_TRACE((" Total Vertex Data Copy                 %10d/%10.4f", gc->asTimes[GLES1_TIMER_VERTEX_DATA_COPY].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_VERTEX_DATA_COPY].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE((" Total Index Data Generate/Copy         %10d/%10.4f", gc->asTimes[GLES1_TIMER_INDEX_DATA_GENERATE_COPY].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_INDEX_DATA_GENERATE_COPY].ui32Total*gc->fCPUSpeed/ui32Frames));

		if(gc->asTimes[GLES1_TIMER_ARRAY_POINTS_TIME].ui32Count) 		{PVR_TRACE((" Array Points                           %10d/%10.4f", gc->asTimes[GLES1_TIMER_ARRAY_POINTS_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ARRAY_POINTS_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ARRAY_LINES_TIME].ui32Count)	 		{PVR_TRACE((" Array Lines                            %10d/%10.4f", gc->asTimes[GLES1_TIMER_ARRAY_LINES_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ARRAY_LINES_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ARRAY_LINE_LOOP_TIME].ui32Count)	 	{PVR_TRACE((" Array Line Loops                       %10d/%10.4f", gc->asTimes[GLES1_TIMER_ARRAY_LINE_LOOP_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ARRAY_LINE_LOOP_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ARRAY_LINE_STRIP_TIME].ui32Count) 	{PVR_TRACE((" Array Line Strips                      %10d/%10.4f", gc->asTimes[GLES1_TIMER_ARRAY_LINE_STRIP_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ARRAY_LINE_STRIP_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ARRAY_TRIANGLES_TIME].ui32Count)	 	{PVR_TRACE((" Array Triangles                        %10d/%10.4f", gc->asTimes[GLES1_TIMER_ARRAY_TRIANGLES_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ARRAY_TRIANGLES_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ARRAY_TRIANGLE_STRIP_TIME].ui32Count){PVR_TRACE((" Array Strips                           %10d/%10.4f", gc->asTimes[GLES1_TIMER_ARRAY_TRIANGLE_STRIP_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ARRAY_TRIANGLE_STRIP_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ARRAY_TRIANGLE_FAN_TIME].ui32Count)	{PVR_TRACE((" Array Fans                             %10d/%10.4f", gc->asTimes[GLES1_TIMER_ARRAY_TRIANGLE_FAN_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ARRAY_TRIANGLE_FAN_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		
		if(gc->asTimes[GLES1_TIMER_ELEMENT_POINTS_TIME].ui32Count) 		  {PVR_TRACE((" Element Points                         %10d/%10.4f", gc->asTimes[GLES1_TIMER_ELEMENT_POINTS_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ELEMENT_POINTS_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ELEMENT_LINES_TIME].ui32Count)	 	  {PVR_TRACE((" Element Lines                          %10d/%10.4f", gc->asTimes[GLES1_TIMER_ELEMENT_LINES_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ELEMENT_LINES_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ELEMENT_LINE_LOOP_TIME].ui32Count)	  {PVR_TRACE((" Element Line Loops                     %10d/%10.4f", gc->asTimes[GLES1_TIMER_ELEMENT_LINE_LOOP_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ELEMENT_LINE_LOOP_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ELEMENT_LINE_STRIP_TIME].ui32Count) 	  {PVR_TRACE((" Element Line Strips                    %10d/%10.4f", gc->asTimes[GLES1_TIMER_ELEMENT_LINE_STRIP_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ELEMENT_LINE_STRIP_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ELEMENT_TRIANGLES_TIME].ui32Count)	  {PVR_TRACE((" Element Triangles                      %10d/%10.4f", gc->asTimes[GLES1_TIMER_ELEMENT_TRIANGLES_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ELEMENT_TRIANGLES_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ELEMENT_TRIANGLE_STRIP_TIME].ui32Count){PVR_TRACE((" Element Strips                         %10d/%10.4f", gc->asTimes[GLES1_TIMER_ELEMENT_TRIANGLE_STRIP_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ELEMENT_TRIANGLE_STRIP_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}
		if(gc->asTimes[GLES1_TIMER_ELEMENT_TRIANGLE_FAN_TIME].ui32Count)  {PVR_TRACE((" Element Fans                           %10d/%10.4f", gc->asTimes[GLES1_TIMER_ELEMENT_TRIANGLE_FAN_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_ELEMENT_TRIANGLE_FAN_TIME].ui32Total * gc->fCPUSpeed/ui32Frames));}


		PVR_TRACE((" "));

		PVR_TRACE((" PDS Variant hit/miss totals"));
		PVR_TRACE((" PDSPixelShaderProgram - variant hit     %10d", gc->asTimes[GLES1_TIMER_PDSVARIANT_HIT_COUNT].ui32Count));
		PVR_TRACE((" PDSPixelShaderProgram - variant miss    %10d", gc->asTimes[GLES1_TIMER_PDSVARIANT_MISS_COUNT].ui32Count));

		PVR_TRACE((" "));


		PVR_TRACE(("         Statistics per call            [Maximum time (ms) in a single call]"));
		PVR_TRACE((" Max Prepare to draw                   %10f", gc->asTimes[GLES1_TIMER_PREPARE_TO_DRAW_TIME].ui32Max*gc->fCPUSpeed));
		PVR_TRACE((" Max SGXKickTA                         %10f", gc->asTimes[GLES1_TIMER_SGXKICKTA_TIME].ui32Max*gc->fCPUSpeed));
		PVR_TRACE((" Max Wait for 3D                       %10f", gc->asTimes[GLES1_TIMER_WAITING_FOR_3D_TIME].ui32Max*gc->fCPUSpeed));
		PVR_TRACE((" Max Wait for TA                       %10f", gc->asTimes[GLES1_TIMER_WAITING_FOR_TA_TIME].ui32Max*gc->fCPUSpeed));
		PVR_TRACE((" Max ValidateState()                   %10f", gc->asTimes[GLES1_TIMER_VALIDATE_TIME].ui32Max*gc->fCPUSpeed));
		PVR_TRACE((" Max EmitState()                       %10f", gc->asTimes[GLES1_TIMER_STATE_EMIT_TIME].ui32Max*gc->fCPUSpeed));
		PVR_TRACE((" Max USE Codeheap Alloc - Fragment     %10f", gc->asTimes[GLES1_TIMER_USECODEHEAP_FRAG_TIME].ui32Max*gc->fCPUSpeed));
		PVR_TRACE((" Max USE Codeheap Alloc - Vertex       %10f", gc->asTimes[GLES1_TIMER_USECODEHEAP_VERT_TIME].ui32Max*gc->fCPUSpeed));
		PVR_TRACE((" Max NamesArray operation              %10f", gc->asTimes[GLES1_TIMER_NAMES_ARRAY].ui32Max*gc->fCPUSpeed));
		PVR_TRACE((" Total Code Heap operations             %10d/%10.4f", gc->asTimes[GLES1_TIMER_CODE_HEAP_TIME].ui32Count/ui32Frames, gc->asTimes[GLES1_TIMER_CODE_HEAP_TIME].ui32Total*gc->fCPUSpeed/ui32Frames));
		PVR_TRACE((" "));

		PVR_TRACE((" Buffer Statistics                           [  kB/kB per Frame  ]"));

		fkB = (IMG_FLOAT)gc->asTimes[GLES1_TIMER_VDM_CTRL_STATE_COUNT].ui32Total;
		fkB /= 256.0f;
		PVR_TRACE(("  VDM Control                           %10.4f/%10.4f", fkB, fkB/ui32Frames ));


		fkB = (IMG_FLOAT)gc->asTimes[GLES1_TIMER_VERTEX_DATA_COUNT].ui32Total;
		fkB /= 256.0f;
		PVR_TRACE(("  Vertex data                           %10.4f/%10.4f", fkB, fkB/ui32Frames ));

		fkB = (IMG_FLOAT)gc->asTimes[GLES1_TIMER_INDEX_DATA_COUNT].ui32Total;
		fkB /= 256.0f;
		PVR_TRACE(("  Index data                            %10.4f/%10.4f", fkB, fkB/ui32Frames ));

		fkB = (IMG_FLOAT)gc->asTimes[GLES1_TIMER_PDS_VERT_DATA_COUNT].ui32Total;
		fkB /= 256.0f;
		PVR_TRACE(("  PDS vertex data                       %10.4f/%10.4f", fkB, fkB/ui32Frames ));

		if(gc->sAppHints.bEnableStaticPDSVertex)
		{
			fkB = (IMG_FLOAT)gc->asTimes[GLES1_TIMER_PDS_VERT_AUXILIARY_COUNT].ui32Total;
			fkB /= 256.0f;
			PVR_TRACE(("  PDS vertex auxiliary data             %10.4f/%10.4f", fkB, fkB/ui32Frames ));
		}

		if(gc->sAppHints.bEnableStaticMTECopy)
		{
			fkB = (IMG_FLOAT)gc->asTimes[GLES1_TIMER_MTE_COPY_COUNT].ui32Total;
			/* 
				the buffer has been fully pre-filled at init context time,
				that is why we add here the full buffer size 
			*/
			fkB += (gc->sAppHints.ui32DefaultPregenMTECopyBufferSize >> 2);
			fkB /= 256.0f;
			PVR_TRACE(("  MTE Copy data                         %10.4f/%10.4f", fkB, fkB/ui32Frames ));
		}

		fkB = (IMG_FLOAT)gc->asTimes[GLES1_TIMER_PDS_FRAG_DATA_COUNT].ui32Total;
		fkB /= 256.0f;
		PVR_TRACE(("  PDS fragment data                     %10.4f/%10.4f", fkB, fkB/ui32Frames ));


		fkB = (IMG_FLOAT)gc->asTimes[GLES1_TIMER_USSE_FRAG_DATA_COUNT].ui32Total;
		fkB /= 256.0f;
		PVR_TRACE(("  USSE fragment data                    %10.4f/%10.4f", fkB, fkB/ui32Frames ));


		PVR_TRACE((" "));

		fkB = (IMG_FLOAT)gc->asTimes[GLES1_TIMER_MTE_STATE_COUNT].ui32Total;
		fkB /= 256.0f;
		PVR_TRACE(("  MTE State                             %10.4f/%10.4f", fkB, fkB/ui32Frames ));

		PVR_TRACE((" "));

		fkB = (IMG_FLOAT)gc->asTimes[GLES1_TIMER_USECODEHEAP_FRAG_COUNT].ui32Total;
		fkB /= 256.0f;
		PVR_TRACE(("  USE Codeheap - Fragment               %10.4f/%10.4f", fkB, fkB/ui32Frames ));

		fkB = (IMG_FLOAT)gc->asTimes[GLES1_TIMER_USECODEHEAP_VERT_COUNT].ui32Total;
		fkB /= 256.0f;
		PVR_TRACE(("  USE Codeheap - Vertex                 %10.4f/%10.4f", fkB, fkB/ui32Frames ));


		PVR_TRACE((" "));

		if(gc->asTimes[GLES1_TIMER_FFGEN_GENERATION_TIME].ui32Total || gc->asTimes[GLES1_TIMER_USC_TIME].ui32Total)
		{
			PVR_TRACE((" FFGen stats                     [  Calls/Time per call  ]"));

			if(gc->asTimes[GLES1_TIMER_FFGEN_GENERATION_TIME].ui32Total)
			{
				PVR_TRACE(("  FFGen time                   %10d/%10.4f", gc->asTimes[GLES1_TIMER_FFGEN_GENERATION_TIME].ui32Count, gc->asTimes[GLES1_TIMER_FFGEN_GENERATION_TIME].ui32Total*gc->fCPUSpeed/gc->asTimes[GLES1_TIMER_FFGEN_GENERATION_TIME].ui32Count));
			}

			if(gc->asTimes[GLES1_TIMER_USC_TIME].ui32Total)
			{
				PVR_TRACE(("  USC time                     %10d/%10.4f", gc->asTimes[GLES1_TIMER_USC_TIME].ui32Count, gc->asTimes[GLES1_TIMER_USC_TIME].ui32Total*gc->fCPUSpeed/gc->asTimes[GLES1_TIMER_USC_TIME].ui32Count));
			}

			PVR_TRACE((" "));
		}

		if(gc->asTimes[GLES1_TIMES_glTexImage2D].ui32Total)
		{
			fLoadCall = gc->asTimes[GLES1_TIMES_glTexImage2D].ui32Total*gc->fCPUSpeed/
				gc->asTimes[GLES1_TIMES_glTexImage2D].ui32Count;
			fLoadFrame = gc->asTimes[GLES1_TIMES_glTexImage2D].ui32Total*gc->fCPUSpeed/ui32Frames;
			fLoadRate = gc->asTimes[GLES1_TIMES_glTexImage2D].fStack/
				(gc->asTimes[GLES1_TIMES_glTexImage2D].ui32Total*gc->fCPUSpeed);
			PVR_TRACE((" Average TexImage Load MTexels/s                 %10.4f", fLoadRate));
		}

		if(gc->asTimes[GLES1_TIMES_glTexSubImage2D].ui32Total)
		{
			fSubLoadCall = gc->asTimes[GLES1_TIMES_glTexSubImage2D].ui32Total*gc->fCPUSpeed/
				gc->asTimes[GLES1_TIMES_glTexSubImage2D].ui32Count;
			fSubLoadFrame = gc->asTimes[GLES1_TIMES_glTexSubImage2D].ui32Total*gc->fCPUSpeed/ui32Frames;
			fSubLoadRate = gc->asTimes[GLES1_TIMES_glTexSubImage2D].fStack/
				(gc->asTimes[GLES1_TIMES_glTexSubImage2D].ui32Total*gc->fCPUSpeed);
			PVR_TRACE((" Average TexSubImage Load MTexels/s              %10.4f", fSubLoadRate));
		}

		if(gc->asTimes[GLES1_TIMES_glCompressedTexImage2D].ui32Total)
		{
			fCompLoadCall = gc->asTimes[GLES1_TIMES_glCompressedTexImage2D].ui32Total*gc->fCPUSpeed/
				gc->asTimes[GLES1_TIMES_glCompressedTexImage2D].ui32Count;
			fCompLoadFrame = gc->asTimes[GLES1_TIMES_glCompressedTexImage2D].ui32Total*gc->fCPUSpeed/ui32Frames;
			fCompLoadRate = gc->asTimes[GLES1_TIMES_glCompressedTexImage2D].fStack/
				(gc->asTimes[GLES1_TIMES_glCompressedTexImage2D].ui32Total*gc->fCPUSpeed);
			PVR_TRACE((" Average Compress Load MTexels/s                 %10.4f", fCompLoadRate));
		}

		if(gc->asTimes[GLES1_TIMES_glCompressedTexSubImage2D].ui32Total)
		{
			fCompSubLoadCall = gc->asTimes[GLES1_TIMES_glCompressedTexSubImage2D].ui32Total*gc->fCPUSpeed/
				gc->asTimes[GLES1_TIMES_glCompressedTexSubImage2D].ui32Count;
			fCompSubLoadFrame = gc->asTimes[GLES1_TIMES_glCompressedTexSubImage2D].ui32Total*gc->fCPUSpeed/ui32Frames;
			fCompSubLoadRate = gc->asTimes[GLES1_TIMES_glCompressedTexSubImage2D].fStack/
				(gc->asTimes[GLES1_TIMES_glCompressedTexSubImage2D].ui32Total*gc->fCPUSpeed);
			PVR_TRACE((" Average Compress Sub Load MTexels/s             %10.4f", fCompSubLoadRate));
		}

		if(gc->asTimes[GLES1_TIMES_glCopyTexImage2D].ui32Total)
		{
			fCopyImgCall = gc->asTimes[GLES1_TIMES_glCopyTexImage2D].ui32Total*gc->fCPUSpeed/
				gc->asTimes[GLES1_TIMES_glCopyTexImage2D].ui32Count;
			fCopyImgFrame = gc->asTimes[GLES1_TIMES_glCopyTexImage2D].ui32Total*gc->fCPUSpeed/ui32Frames;
		}

		if(gc->asTimes[GLES1_TIMES_glCopyTexSubImage2D].ui32Total)
		{
			fCopyImgCall = gc->asTimes[GLES1_TIMES_glCopyTexSubImage2D].ui32Total*gc->fCPUSpeed/
				gc->asTimes[GLES1_TIMES_glCopyTexSubImage2D].ui32Count;
			fCopyImgFrame = gc->asTimes[GLES1_TIMES_glCopyTexSubImage2D].ui32Total*gc->fCPUSpeed/ui32Frames;
		}

		if(gc->asTimes[GLES1_TIMER_TEXTURE_ALLOCATE_TIME].ui32Total)
		{
			fTexAllocateCall = gc->asTimes[GLES1_TIMER_TEXTURE_ALLOCATE_TIME].ui32Total*gc->fCPUSpeed/
				gc->asTimes[GLES1_TIMER_TEXTURE_ALLOCATE_TIME].ui32Count;
			fTexAllocateFrame = gc->asTimes[GLES1_TIMER_TEXTURE_ALLOCATE_TIME].ui32Total*gc->fCPUSpeed/ui32Frames;
		}

		if(gc->asTimes[GLES1_TIMER_TEXTURE_TRANSLATE_LOAD_TIME].ui32Total)
		{
			fTexTranslateCall = gc->asTimes[GLES1_TIMER_TEXTURE_TRANSLATE_LOAD_TIME].ui32Total*gc->fCPUSpeed/
				gc->asTimes[GLES1_TIMER_TEXTURE_TRANSLATE_LOAD_TIME].ui32Count;
			fTexTranslateFrame = gc->asTimes[GLES1_TIMER_TEXTURE_TRANSLATE_LOAD_TIME].ui32Total*gc->fCPUSpeed/ui32Frames;
			/* fTexTranslateRate = gc->asTimes[GLES1_TIMER_TEXTURE_TRANSLATE_LOAD_TIME].fStack/
				(gc->asTimes[GLES1_TIMER_TEXTURE_TRANSLATE_LOAD_TIME].ui32Total*gc->fCPUSpeed); */
		}

		if(gc->asTimes[GLES1_TIMER_TEXTURE_GHOST_LOAD_TIME].ui32Total)
		{
			fTexLoadGhostCall = gc->asTimes[GLES1_TIMER_TEXTURE_GHOST_LOAD_TIME].ui32Total*gc->fCPUSpeed/
				gc->asTimes[GLES1_TIMER_TEXTURE_GHOST_LOAD_TIME].ui32Count;
			fTexLoadGhostFrame = gc->asTimes[GLES1_TIMER_TEXTURE_GHOST_LOAD_TIME].ui32Total*gc->fCPUSpeed/ui32Frames;
		}

		if(gc->asTimes[GLES1_TIMER_TEXTURE_READBACK_TIME].ui32Total)
		{
			fTexReadbackCall = gc->asTimes[GLES1_TIMER_TEXTURE_READBACK_TIME].ui32Total*gc->fCPUSpeed/
				gc->asTimes[GLES1_TIMER_TEXTURE_READBACK_TIME].ui32Count;
			fTexReadbackFrame = gc->asTimes[GLES1_TIMER_TEXTURE_READBACK_TIME].ui32Total*gc->fCPUSpeed/ui32Frames;
		}

		PVR_TRACE((" Texture Statistics         [  Calls/perCall-Time|Texels  / perFrame-Time/Texels  ]"));
		PVR_TRACE(("    TexImage                 %7d/%12.4f/%8.0f/%14.4f/%8.2f",
				   gc->asTimes[GLES1_TIMES_glTexImage2D].ui32Count,
				   fLoadCall, GLES1_PIXELS_PER_CALL(GLES1_TIMES_glTexImage2D),
				   fLoadFrame, GLES1_PIXELS_PER_FRAME(GLES1_TIMES_glTexImage2D)));
		PVR_TRACE(("    TexSubImage              %7d/%12.4f/%8.0f/%14.4f/%8.2f",
				   gc->asTimes[GLES1_TIMES_glTexSubImage2D].ui32Count,
				   fSubLoadCall, GLES1_PIXELS_PER_CALL(GLES1_TIMES_glTexSubImage2D),
				   fSubLoadFrame, GLES1_PIXELS_PER_FRAME(GLES1_TIMES_glTexSubImage2D)));
		if(fCompLoadCall)
		{
			PVR_TRACE(("    CompTex                  %7d/%12.4f/%8.0f/%14.4f/%8.2f",
				   gc->asTimes[GLES1_TIMES_glCompressedTexImage2D].ui32Count,
				   fCompLoadCall, GLES1_PIXELS_PER_CALL(GLES1_TIMES_glCompressedTexImage2D),
				   fCompLoadFrame, GLES1_PIXELS_PER_FRAME(GLES1_TIMES_glCompressedTexImage2D)));
		}
		if(fCompSubLoadCall)
		{
			PVR_TRACE(("    CompSubTex               %7d/%12.4f/%8.0f/%14.4f/%8.2f",
				   gc->asTimes[GLES1_TIMES_glCompressedTexSubImage2D].ui32Count,
				   fCompSubLoadCall, GLES1_PIXELS_PER_CALL(GLES1_TIMES_glCompressedTexSubImage2D),
				   fCompSubLoadFrame, GLES1_PIXELS_PER_FRAME(GLES1_TIMES_glCompressedTexSubImage2D)));
		}

		PVR_TRACE((" Texture Statistics         [  Calls/Time per Call/Time per Frame]"));
		PVR_TRACE(("    CopyImage                %7d/%13.4f/%15.4f",
				   gc->asTimes[GLES1_TIMES_glCopyTexImage2D].ui32Count, fCopyImgCall, fCopyImgFrame));
		PVR_TRACE(("    CopySubImage             %7d/%13.4f/%15.4f",
				   gc->asTimes[GLES1_TIMES_glCopyTexSubImage2D].ui32Count, fCopySubImgCall, fCopySubImgFrame));
		PVR_TRACE(("    AllocateTexture          %7d/%13.4f/%15.4f",
				   gc->asTimes[GLES1_TIMER_TEXTURE_ALLOCATE_TIME].ui32Count, fTexAllocateCall, fTexAllocateFrame));
		PVR_TRACE(("    TranslateLoadTexture     %7d/%13.4f/%15.4f",
				   gc->asTimes[GLES1_TIMER_TEXTURE_TRANSLATE_LOAD_TIME].ui32Count, fTexTranslateCall, fTexTranslateFrame));
		PVR_TRACE(("    Load Ghost Texture       %7d/%13.4f/%15.4f",
				   gc->asTimes[GLES1_TIMER_TEXTURE_GHOST_LOAD_TIME].ui32Count, fTexLoadGhostCall, fTexLoadGhostFrame));
		PVR_TRACE(("    Texture Readback         %7d/%13.4f/%15.4f",
				   gc->asTimes[GLES1_TIMER_TEXTURE_READBACK_TIME].ui32Count, fTexReadbackCall, fTexReadbackFrame));


		/* Output read pixels stats */
		if(gc->asTimes[GLES1_TIMES_glReadPixels].ui32Count)
		{
			PVR_TRACE((" Read Pixel Stats  [  Calls/perCall-Time|Pixels  / perFrame-Time|Pixels  ]"));
			PVR_TRACE(("    Color           %7d/%12.4f/%8.0f/%14.4f/%8.2f",
						   GLES1_CALLS(GLES1_TIMES_glReadPixels),
						   GLES1_TIME_PER_CALL(GLES1_TIMES_glReadPixels),
						   GLES1_PIXELS_PER_CALL(GLES1_TIMES_glReadPixels),
						   GLES1_TIME_PER_FRAME(GLES1_TIMES_glReadPixels),
						   GLES1_PIXELS_PER_FRAME(GLES1_TIMES_glReadPixels)));
		}
	}

	PVR_TRACE((""));

	PVR_TRACE((" Texture allocation HWM = %u bytes",ui32TextureMemHWM));

#if defined(SCENE_MEMORY_METRICS)
	if(gc->sSceneMemoryMetrics.ui32Count) 
	{
		SceneMemoryMetricsOutput(gc);
	}
#endif

	if (gc->sAppHints.bDumpProfileData)
	{
		ProfileOutputTotals(gc);
	}
}

#else /* defined(TIMING) || defined(DEBUG) */


#endif /* defined(TIMING) || defined(DEBUG) */

/******************************************************************************
 End of file (metrics.c)
******************************************************************************/



