/*!****************************************************************************
@File           pvr2dblt3d.c

@Title          PVR2D library 3D blit

@Author         Imagination Technologies

@Date           14/12/2006

@Copyright      Copyright 2006-2009 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or otherwise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform       Generic

@Description    PVR2D library 3D blit for scaling and custom use code

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: pvr2dblt3d.c $
******************************************************************************/

/* PRQA S 5013++ */ /* disable basic types warning */

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#include "img_defs.h"
#include "services.h"
#include "pvr2d.h"
#include "pvr2dint.h"
#include "sgxapi.h"

/*****************************************************************************
 @Function	PVR2DLoadUseCode

 @Input		hContext : PVR2D context handle

 @Input		pUseCode : USSE instruction bytes to load into device memory

 @Input		UseCodeSize : number of USSE instruction bytes to load into device memory

 @Output	pUseCodeHandle : handle of loaded USSE code, for use as input to PVR2DBlt3D()

 @Return	error : PVR2D error code

 @Description : loads USSE code to be executed for each pixel during a 3DBlt
******************************************************************************/
// PVR2DUFreeUseCode must be called to free up the device memory.
PVR2D_EXPORT
PVR2DERROR PVR2DLoadUseCode (const PVR2DCONTEXTHANDLE hContext, const PVR2D_UCHAR	*pUseCode,
									const PVR2D_ULONG UseCodeSize, PVR2D_HANDLE *pUseCodeHandle)
{
	PVR2D_DPF((PVR_DBG_ERROR, "3D Core blits are not supported on this platform!"));
	return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
}


/*****************************************************************************
 @Function	PVR2DFreeUseCode

 @Input		hContext : PVR2D context handle

 @Input		hUseCodeHandle : handle of loaded USSE code to free (unload)

 @Return	error : PVR2D error code

 @Description : unloads USSE code
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DFreeUseCode (const PVR2DCONTEXTHANDLE hContext, const PVR2D_HANDLE hUseCodeHandle)
{
	PVR2D_DPF((PVR_DBG_ERROR, "3D Core blits are not supported on this platform!"));
	return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
}


/*****************************************************************************
 @Function	PVR2DBlt3D

 @Input		hContext : PVR2D context handle

 @Input		pBlt3D : Structure containing blt parameters

 @Return	error : PVR2D error code

 @Description : Does blt using the 3D Core, supports scaling and custom USSE code
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DBlt3D (const PVR2DCONTEXTHANDLE hContext, const PPVR2D_3DBLT pBlt3D)
{
	PVR2D_DPF((PVR_DBG_ERROR, "3D Core blits are not supported on this platform!"));
	return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
}

/*****************************************************************************
 @Function	PVR2DBlt3DExt

 @Input		hContext : PVR2D context handle

 @Input		pBlt3D : Structure containing blt parameters

 @Return	error : PVR2D error code

 @Description : Does blt using the 3D Core, supports scaling and custom USSE code
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DBlt3DExt (const PVR2DCONTEXTHANDLE hContext, const PPVR2D_3DBLT_EXT pBlt3D)
{
	PVR2D_DPF((PVR_DBG_ERROR, "3D Core blits are not supported on this platform!"));
	return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
}