/******************************************************************************
 * Name         : texstream.c
 * Author       : jml
 * Created      : 15/01/2007
 *
 * Copyright    : 2007-2009 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means,electronic, mechanical, manual or otherwise, 
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, 
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Description	: Texture stream extension
 *
 * Modifications:-
 * $Log: texstream.c $
 **************************************************************************/

#include "context.h"

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)




/***********************************************************************************
 Function Name      : GetNumStreamDevices
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_UINT32 GetNumStreamDevices(GLES1Context *gc)
{
	PVRSRV_ERROR eResult;
	IMG_UINT32 ui32DevCount;

	/* how many buffer class devices? */
	eResult = PVRSRVEnumerateDeviceClass (gc->psSysContext->psConnection,
						  PVRSRV_DEVICE_CLASS_BUFFER, 
						  &ui32DevCount,
						  IMG_NULL); 

	/* No more work if no buffer class devices */
	if(eResult != PVRSRV_OK)
	{
		return 0;
	}

	return ui32DevCount;
}


/***********************************************************************************
 Function Name      : FreeTexStreamState
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL PVRSRV_ERROR FreeTexStreamState(GLES1Context *gc)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 i;

	GLES1StreamDevice *psBufferDevice = gc->psBufferDevice;
	GLES1StreamDevice *psPreviousDevice;

	while(psBufferDevice)
	{
		for(i=0; i < psBufferDevice->sBufferInfo.ui32BufferCount; i++)
		{
			PVRSRVUnmapDeviceClassMemory (&gc->psSysContext->s3D, psBufferDevice->psBuffer[i].psBufferSurface);
		}
		
		if(psBufferDevice->psBuffer)
		{
			GLES1Free(gc, psBufferDevice->psBuffer);
		}

		/* close the 3rd party buffer API */
		eError = PVRSRVCloseBCDevice(gc->psSysContext->psConnection, psBufferDevice->hBufferDevice);

		if(eError != PVRSRV_OK)
		{
			return eError;	
		}

		/* advance to next device and delete the old */
		psPreviousDevice = psBufferDevice;
		psBufferDevice = psBufferDevice->psNext;

		GLES1Free(gc, psPreviousDevice->psExtTexState);
		GLES1Free(gc, psPreviousDevice);
	}

	return PVRSRV_OK;
}

/***********************************************************************************
 Function Name      : CreateTextureStreamState
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static PVRSRV_ERROR CreateTextureStreamState(GLES1StreamDevice *psBufferDevice)
{
	return CreateExternalTextureState(psBufferDevice->psExtTexState, psBufferDevice->sBufferInfo.ui32Width,
							psBufferDevice->sBufferInfo.ui32Height, psBufferDevice->sBufferInfo.pixelformat,
							psBufferDevice->sBufferInfo.ui32ByteStride, psBufferDevice->sBufferInfo.ui32Flags);
}


/***********************************************************************************
 Function Name      : FindBufferDevice
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_BOOL FindBufferDevice(GLES1Context *gc, IMG_UINT32 device, GLES1StreamDevice **ppsBufferDevice)
{
	GLES1StreamDevice *psBufferDevice = gc->psBufferDevice;

	while(psBufferDevice)
	{
		if(psBufferDevice->ui32BufferDevice == device)
		{
			if(psBufferDevice->bGhosted == IMG_FALSE)
			{
				*ppsBufferDevice = psBufferDevice;

				return IMG_TRUE;
			}
			else
			{
				/* Otherwise a new psBufferDevice should be created */
				return IMG_FALSE;
			}
		}

		/* advance to next device */
		psBufferDevice = psBufferDevice->psNext;
	}

	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : AddBufferDevice
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_BOOL AddBufferDevice(GLES1Context *gc, IMG_UINT32 device, GLES1StreamDevice **ppsBufferDevice)
{
	GLES1StreamDevice *psBufferDevice;
	PVRSRV_ERROR eResult;
	IMG_UINT32 i;
	IMG_UINT32 ui32DevCount, *pui32DeviceID;
	
#ifdef DEBUG
	if(FindBufferDevice(gc, device, &psBufferDevice))
	{
		PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: device %d already present", device));
		return IMG_FALSE;
	}
#endif

	psBufferDevice = GLES1Calloc(gc, sizeof(GLES1StreamDevice));

	if(!psBufferDevice)
	{
		PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: Failed to alloc device struct"));
		return IMG_FALSE;
	}

	/* Initialise ui32BufferRefCount */
	psBufferDevice->ui32BufferRefCount = 0;

	/* Initialise bGhosted */
	psBufferDevice->bGhosted = IMG_FALSE;


	pui32DeviceID = IMG_NULL;

	psBufferDevice->psExtTexState = GLES1Calloc(gc, sizeof(GLES1ExternalTexState));
	if(!psBufferDevice->psExtTexState)
	{
		PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: Failed to alloc device YUV info struct"));
		goto ErrorCase;
	}

	
	/* how many buffer class devices? */
	eResult = PVRSRVEnumerateDeviceClass (gc->psSysContext->psConnection,
											PVRSRV_DEVICE_CLASS_BUFFER,
											&ui32DevCount,
											IMG_NULL);

	/* No more work if no buffer class devices */
	if(eResult != PVRSRV_OK || ui32DevCount == 0)
	{
		PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: No buffer class devices"));
		goto ErrorCase;
	}

	/* get the device ids for the devices */
	pui32DeviceID = GLES1Calloc(gc, sizeof(IMG_UINT32)*ui32DevCount);

	if(!pui32DeviceID)
	{
		PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: Failed to alloc pui32DeviceID"));
		/* eResult = PVRSRV_ERROR_OUT_OF_MEMORY; */
		goto ErrorCase;
	}

	eResult = PVRSRVEnumerateDeviceClass (gc->psSysContext->psConnection,
										PVRSRV_DEVICE_CLASS_BUFFER,
										&ui32DevCount,
										pui32DeviceID);
	if(eResult != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: EnumerateDeviceClass failed"));
		goto ErrorCase;
	}

	for(i=0; i < ui32DevCount; i++)
	{
		/* try opening the device by index */
		psBufferDevice->hBufferDevice = PVRSRVOpenBCDevice ( &gc->psSysContext->s3D, pui32DeviceID[i]);
	
		if (psBufferDevice->hBufferDevice == IMG_NULL)
		{
			PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: OpenBufferClassDevice failed"));

			/* eResult = PVRSRV_ERROR_INIT_FAILURE; */

			goto ErrorCase;
		}
		
		eResult = PVRSRVGetBCBufferInfo(psBufferDevice->hBufferDevice, &psBufferDevice->sBufferInfo);
		
		if(eResult != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: BufferClassGetBufferInfo failed"));
			goto ErrorCase;
		}

		if(psBufferDevice->sBufferInfo.ui32BufferDeviceID == device)
		{
			break;
		}

		/* close the 3rd party buffer API */
		eResult = PVRSRVCloseBCDevice(gc->psSysContext->psConnection, psBufferDevice->hBufferDevice);

		if(eResult != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: CloseBufferClassDevice failed"));
			goto ErrorCase;
		}
	}

	GLES1Free(gc, pui32DeviceID);

	pui32DeviceID = IMG_NULL;

	if(i == ui32DevCount)
	{
		PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: Invalid buffer device id"));

		/* eResult = PVRSRV_ERROR_INIT_FAILURE; */

		goto ErrorCase;
	}

	/* record the device index in the buffer device info structure */
	psBufferDevice->ui32BufferDevice = device;

	/* CreateTextureStreamState and validate the buffer */
	eResult = CreateTextureStreamState(psBufferDevice);

	if(eResult != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: CreateTextureStreamState failed"));

		goto ErrorCase;
	}

	psBufferDevice->psBuffer = GLES1Calloc(gc, sizeof(GLES1DeviceBuffer) * psBufferDevice->sBufferInfo.ui32BufferCount);

	if(!psBufferDevice->psBuffer)
	{
		PVR_DPF((PVR_DBG_ERROR,"AddBufferDevice: Failed to alloc stream struct"));

		/* eResult = PVRSRV_ERROR_OUT_OF_MEMORY; */

		goto ErrorCase;
	}

	for(i=0; i < psBufferDevice->sBufferInfo.ui32BufferCount; i++)
	{

		eResult = PVRSRVGetBCBuffer (psBufferDevice->hBufferDevice,
			   								i,	
											&psBufferDevice->psBuffer[i].hBuffer);

		if(eResult != PVRSRV_OK)
		{
			goto ErrorCase;
		}


		 eResult = PVRSRVMapDeviceClassMemory (&gc->psSysContext->s3D,
		 							gc->psSysContext->hDevMemContext,
									psBufferDevice->psBuffer[i].hBuffer,
									&psBufferDevice->psBuffer[i].psBufferSurface);

		if(eResult != PVRSRV_OK)
		{
			goto ErrorCase;
		}

		if(psBufferDevice->psBuffer[i].psBufferSurface->sDevVAddr.uiAddr & (EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT - 1))
		{
			PVR_DPF((PVR_DBG_ERROR,"Unsupported buffer class buffer address granularity"));

			/* eResult = PVRSRV_ERROR_INIT_FAILURE; */

			goto ErrorCase;
		}

		psBufferDevice->psBuffer[i].hBuffer = (IMG_HANDLE)i;
		psBufferDevice->psBuffer[i].ui32ByteStride = psBufferDevice->sBufferInfo.ui32ByteStride;
		psBufferDevice->psBuffer[i].ui32PixelWidth = psBufferDevice->sBufferInfo.ui32Width;
		psBufferDevice->psBuffer[i].ui32PixelHeight = psBufferDevice->sBufferInfo.ui32Height;
		psBufferDevice->psBuffer[i].ePixelFormat = psBufferDevice->sBufferInfo.pixelformat;
	}

	/* insert the Device into the head of list */
	psBufferDevice->psNext = gc->psBufferDevice;
	gc->psBufferDevice = psBufferDevice;

	/* copy back the device info. */
	*ppsBufferDevice = psBufferDevice;

	return IMG_TRUE;

ErrorCase:

	if (psBufferDevice->psBuffer)
	{
		for(i=0; i < psBufferDevice->sBufferInfo.ui32BufferCount; i++)
		{
			if(psBufferDevice->psBuffer[i].psBufferSurface)
			{
				PVRSRVUnmapDeviceClassMemory (&gc->psSysContext->s3D, psBufferDevice->psBuffer[i].psBufferSurface);
			}
		}

		GLES1Free(gc, psBufferDevice->psBuffer);
	}	

	if (pui32DeviceID)
	{
		GLES1Free(gc,pui32DeviceID);
	}
	if (psBufferDevice->psExtTexState)
	{
		GLES1Free(gc, psBufferDevice->psExtTexState);
		psBufferDevice->psExtTexState = IMG_NULL;
	}

	GLES1Free(gc, psBufferDevice);

	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : glGetTexStreamDeviceAttributeivIMG
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glGetTexStreamDeviceAttributeivIMG(GLint device, GLenum pname, GLint *params)
{
	GLES1StreamDevice *psBufferDevice;
	
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexStreamDeviceAttributeivIMG"));

	GLES1_TIME_START(GLES1_TIMES_glGetTexStreamDeviceAttributeiv);

	/* find the device */
	if(!FindBufferDevice(gc, (IMG_UINT32)device, &psBufferDevice))
	{
		/* Add the device */
		if(!AddBufferDevice(gc, (IMG_UINT32)device, &psBufferDevice))
		{
			PVR_DPF((PVR_DBG_ERROR,"glGetTexStreamDeviceAttributeivIMG: Can't find or add specified device"));			

			SetError(gc, GL_INVALID_VALUE);
			GLES1_TIME_STOP(GLES1_TIMES_glGetTexStreamDeviceAttributeiv);

			return;	
		}
	}

	switch(pname)
	{
		case GL_TEXTURE_STREAM_DEVICE_WIDTH_IMG:
		{
			*params = (GLint)psBufferDevice->sBufferInfo.ui32Width;

			break;
		}
		case GL_TEXTURE_STREAM_DEVICE_HEIGHT_IMG:
		{
			*params = (GLint)psBufferDevice->sBufferInfo.ui32Height;

			break;
		}
		case GL_TEXTURE_STREAM_DEVICE_FORMAT_IMG:
		{
			switch(psBufferDevice->sBufferInfo.pixelformat)
			{
				case PVRSRV_PIXEL_FORMAT_RGB565:
				case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY:
				case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV:
				case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_VYUY:
				case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YVYU:
				case PVRSRV_PIXEL_FORMAT_NV12:
				case PVRSRV_PIXEL_FORMAT_YV12:
				default:
				{
					*params = GL_RGB;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB8888:
				case PVRSRV_PIXEL_FORMAT_ARGB4444:
				{
					*params = GL_RGBA;

					break;
				}
			}

			break;
		}
		case GL_TEXTURE_STREAM_DEVICE_NUM_BUFFERS_IMG:
		{
			*params = (GLint)psBufferDevice->sBufferInfo.ui32BufferCount;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			break;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glGetTexStreamDeviceAttributeiv);
}


/***********************************************************************************
 Function Name      : glTexBindStreamIMG
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glTexBindStreamIMG(GLint device, GLint deviceoffset)
{
	GLES1StreamDevice *psBufferDevice;
	GLESTexture *psTex;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexBindStreamIMG"));

	GLES1_TIME_START(GLES1_TIMES_glTexBindStream);

	/* find the device */
	if(!FindBufferDevice(gc, (IMG_UINT32)device, &psBufferDevice))
	{
		/* Add the device */
		if(!AddBufferDevice(gc, (IMG_UINT32)device, &psBufferDevice))
		{
			PVR_DPF((PVR_DBG_ERROR,"glTexBindStreamIMG: Can't find or add specified device"));			
bad_val:
			SetError(gc, GL_INVALID_VALUE);

			GLES1_TIME_STOP(GLES1_TIMES_glTexBindStream);

			return;	
		}
	}

	/* check the device offset */
	if((deviceoffset < 0) || ((IMG_UINT32)deviceoffset >= psBufferDevice->sBufferInfo.ui32BufferCount))
	{
		PVR_DPF((PVR_DBG_ERROR,"glTexBindStreamIMG: Bad offset (%d) into this device",deviceoffset));

		goto bad_val;
	}
	
	psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture][GLES1_TEXTURE_TARGET_STREAM];
	
	if(psTex->psBufferDevice != psBufferDevice || psTex->ui32BufferOffset != (IMG_UINT32)deviceoffset)
	{
	    /* Decrement the bound buffer device's RefCount */
	    if (psTex->psBufferDevice)
		{
			psTex->psBufferDevice->ui32BufferRefCount--;
		}

	    /* Increment the new buffer device's RefCount */
		psBufferDevice->ui32BufferRefCount++;

		/* Reset texture's buffer device */
		psTex->psBufferDevice = psBufferDevice;
		psTex->ui32BufferOffset = (IMG_UINT32)deviceoffset;

		SetTextureFormat(psTex, psBufferDevice);

		/* Setup texture mipmap level[0]'s width and height, which are used in glDrawTexOES */
		psTex->psMipLevel[0].ui32Width  = psBufferDevice->psBuffer[deviceoffset].ui32PixelWidth;
		psTex->psMipLevel[0].ui32Height = psBufferDevice->psBuffer[deviceoffset].ui32PixelHeight;

		/* Ensure that newly bound buffer will cause texture to be validated (cheaply) for texture state */
		TextureRemoveResident(gc, psTex);
	}

	GLES1_TIME_STOP(GLES1_TIMES_glTexBindStream);
}


/***********************************************************************************
 Function Name      : glGetTexStreamDeviceNameIMG
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT const GLubyte * GL_APIENTRY glGetTexStreamDeviceNameIMG(GLint device)
{
	GLES1StreamDevice *psBufferDevice;
	__GLES1_GET_CONTEXT_RETURN(IMG_NULL);

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexStreamDeviceNameIMG"));

	GLES1_TIME_START(GLES1_TIMES_glGetTexStreamDeviceName);

	/* find the device */
	if(!FindBufferDevice(gc, (IMG_UINT32)device, &psBufferDevice))
	{
		/* Add the device */
		if(!AddBufferDevice(gc, (IMG_UINT32)device, &psBufferDevice))
		{
			PVR_DPF((PVR_DBG_ERROR,"glGetTexStreamDeviceNameIMG: Can't find or add specified device"));

			SetError(gc, GL_INVALID_VALUE);

			GLES1_TIME_STOP(GLES1_TIMES_glGetTexStreamDeviceName);

			return IMG_NULL;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glGetTexStreamDeviceName);

	return (GLubyte *)psBufferDevice->sBufferInfo.szDeviceName;
}

#endif

/******************************************************************************
 End of file (texstream.c)
******************************************************************************/

