/**************************************************************************
 * Name         : linux_tls.c
 *
 * Copyright    : 2005,2007 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: linux_tls.c $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/
#if !defined(DISABLE_THREADS)
#include <pthread.h> /* Must be first included file */

#include <kernel.h>

#include <img_types.h>
#include <img_defs.h>
#include <pvr_debug.h>

#include "common_tls.h"

/* An explanation for some of the build options in this file:
 *
 * USE_PTHREADS:
 *  - This will use the posix APIs to set and retrieve your TLS pointers.
 * USE_GCC__thread_KEYWORD
 * - This will declare a variable using GCC's __thread keyword which we can
 *   get/set just like any other static global variable. In some cases this
 *   can be a bit faster then using the pthreads API.
 *   (Note we still use the pthreads API in this case too, but only for
 *   looking up thread IDs.)
 *
 * You should use empirical data to determine which option performs better for
 * a specific platform+toolchain combo!
 *
 *
 * If you are using USE_PTHREADS and your toolchain uses (slower)linuxthreads,
 * not NPTL then there is an optional special case optimisation enabled via
 * OPTIMISE_NON_NPTL_SINGLE_THREAD_TLS_LOOKUP. Which means until it is detected
 * that there are multiple threads accessing TLS, then your TLS pointer is
 * stored in a global static variable.
 *  
 * Note: It is important that you _don't_ use this optimisation for APIs that
 * call _GetTLSValue in a new thread before they call _SetTLSValue. This is
 * because calls to _SetTLSValue are the only way to change the value of
 * bSingleThreaded which is used inside _GetTLSValue to determin if the
 * optimisation is currently enabled. At the time of writting that meant don't
 * use this optimisation with EGL!
 */

#if defined(USE_PTHREADS)
static pthread_once_t OnceControl = PTHREAD_ONCE_INIT;
static pthread_key_t TLSKey;

#if OPTIMISE_NON_NPTL_SINGLE_THREAD_TLS_LOOKUP
static pthread_t KeyCreateThreadID; /* The thread ID of the first thread */
static IMG_BOOL bSingleThreaded = IMG_TRUE;
static IMG_VOID *pvTLSThread0;
#endif

#elif defined(USE_GCC__thread_KEYWORD)

//static __thread IMG_VOID *pvTLS;

#else
#error FIXME Choose a TLS implementation method (USE_PTHREADS or USE_GCC__thread_KEYWORD)
#endif


#if defined(USE_PTHREADS)
static void TLSKeyCreate(void)
{
    int iStatus;
    iStatus = pthread_key_create(&TLSKey, NULL);
    if(iStatus != 0)
    {
        PVR_DPF((PVR_DBG_ERROR, "TLSKeyCreate: Failed to create a thread-specific data key\n"));
    }
#if OPTIMISE_NON_NPTL_SINGLE_THREAD_TLS_LOOKUP
    KeyCreateThreadID = pthread_self();
#endif
}

/* The gcc constructor attribute allows us to initialise our TLS key
 * at library load time. */
void __attribute__((constructor))
TLS_PREFIX(_InitialiseTLS)(IMG_VOID)
{
    pthread_once(&OnceControl, TLSKeyCreate);
}
#endif


/***********************************************************************************
 Function Name      : ENV_GetTLSID
 Inputs             : -
 Outputs            : -
 Returns            : The ID of the current thread
 Description        : Gets the current thread ID
************************************************************************************/
IMG_UINT32 TLS_PREFIX(_GetTLSID(IMG_VOID))
{
	return (IMG_UINT32)sceKernelGetThreadId();
}


/***********************************************************************************
 Function Name      : ENV_GetTLSValue
 Inputs             : -
 Outputs            : -
 Returns            : The value from thread local storage, as a void*
 Description        : Gets a value from thread local storage
************************************************************************************/
IMG_VOID* TLS_PREFIX(_GetTLSValue(IMG_VOID))
{
	IMG_VOID *value = *(IMG_VOID **)sceKernelGetTLSAddr(0xFF);
    return value; /* USE_GCC__thread_KEYWORD */
}


/***********************************************************************************
 Function Name      : ENV_SetTLSValue
 Inputs             : psA - the value to set
 Outputs            : -
 Returns            : IMG_TRUE, if successful
 Description        : Sets a value in thread local storage
************************************************************************************/
IMG_BOOL TLS_PREFIX(_SetTLSValue(IMG_VOID *pvA))
{
	IMG_VOID **addr = (IMG_VOID **)sceKernelGetTLSAddr(0xFF);
	*addr = pvA;
    
	return IMG_TRUE;
}

#endif /* !defined(DISABLE_THREADS) */

