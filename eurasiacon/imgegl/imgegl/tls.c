/**************************************************************************
 * Name         : tls.c
 * Author       : BCB
 * Created      : 11/08/2003
 *
 * Copyright    : 2003-2005 by Imagination Technologies Limited. All rights reserved.
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
 * $Date: 2010/11/18 17:21:14 $ $Revision: 1.17 $
 * $Log: tls.c $
 *
 *  --- Revision Logs Removed --- 
 ***************************************************************************/

#include "egl_internal.h"
#include "tls.h"
#include "generic_ws.h"


/***************************************************************************
 *
 *  FUNCTION   : TLS_Open ()
 *  PURPOSE    : Create and initialise our thread local storage if it
 *				 doesn't already exist, otherwise returns the current tls
 *  PARAMETERS : None.
 *  RETURNS    : Pointer to this threads local storage structure.
 *
 ***************************************************************************/
IMG_INTERNAL TLS TLS_Open(IMG_BOOL (*init)(TLS tls))
{
    TLS tls;

    PVR_DPF((PVR_DBG_VERBOSE, "TLS_Open()"));

    tls = (TLS)IMGEGLGetTLSValue();

    if (tls == IMG_NULL)
    {
		tls = EGLCalloc(sizeof(struct tls_tag));

		if (tls!=IMG_NULL)
		{
			if (!init(tls))
			{
				EGLFree(tls);

				return IMG_NULL;
			}
		}

		if (!IMGEGLSetTLSValue(tls))
		{
			EGLFree(tls);

			return IMG_NULL;
		}
    }

    return tls;
}


/****************************************************************************
 *
 *  FUNCTION   : TLS_Close ()
 *  PURPOSE    : Close the thread local storage, when all openers have closed,
 *               thread local storage is deallocated.
 *  PARAMETERS : None.
 *  RETURNS    : Pointer to this threads local storage structure.
 * 
 ***************************************************************************/
IMG_INTERNAL IMG_VOID TLS_Close(IMG_VOID (*deinit)(TLS tls))
{
    TLS tls = (TLS)IMGEGLGetTLSValue();
     
	PVR_DPF((PVR_DBG_VERBOSE, "TLS_Close()"));
  
	if (tls!=IMG_NULL)
    {
		deinit(tls);

		IMGEGLSetTLSValue(IMG_NULL);

		EGLFree(tls);
    }
}

/*****************************************************************************
 End of file (tls.c)
*****************************************************************************/

