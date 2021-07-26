/******************************************************************************
 * Name         : blobcache.h
 *
 * Copyright    : 2011 by Imagination Technologies Limited.
 *                All rights reserved. No part of this software, either
 *                material or conceptual may be copied or distributed,
 *                transmitted, transcribed, stored in a retrieval system or
 *                translated into any human or computer language in any form
 *                by any means, electronic, mechanical, manual or otherwise,
 *                or disclosed to third parties without the express written
 *                permission of Imagination Technologies Limited, Home Park
 *                Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *****************************************************************************/

#ifndef _BLOBCACHE_H_
#define _BLOBCACHE_H_

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE)
void IMGeglSetBlobCacheFuncsANDROID(EGLDisplay eglDpy, 
									EGLSetBlobFunc set, 
									EGLGetBlobFunc get);


#endif

#if defined(__cplusplus)
}
#endif

#endif /*_BLOBCACHE_H_*/

/******************************************************************************
 End of file (blobcache.h)
******************************************************************************/

