#include <kernel.h>

#include "eurasia/include4/services.h"

/******************************************************************************
 Function Name      : PVRSRVMemSet
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVMemSet(IMG_VOID *pvDest, IMG_UINT8 ui8Value, IMG_UINT32 ui32Size)
{
	sceClibMemset(pvDest, (IMG_INT)ui8Value, (size_t)ui32Size);
}