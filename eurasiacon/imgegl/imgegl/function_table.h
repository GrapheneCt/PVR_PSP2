/******************************************************************************
 * Name         : function_table.h
 *
 * Copyright    : 2007 by Imagination Technologies Limited.
 * 				  All rights reserved. No part of this software, either
 * 				  material or conceptual may be copied or distributed,
 * 				  transmitted, transcribed, stored in a retrieval system or
 * 				  translated into any human or computer language in any form
 * 				  by any means, electronic, mechanical, manual or other-wise,
 * 				  or disclosed to third parties without the express written
 * 				  permission of Imagination Technologies Limited, HomePark
 * 				  Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Modifications: 
 * $Log: function_table.h $
 ********************************************************************************/

#ifndef _FUNCTION_TABLE_H_
#define _FUNCTION_TABLE_H_

#include <string.h>
#include "img_defs.h"

#define IMGEGL_NO_LIBRARY    0
#define IMGEGL_NAME_CLASH    1
#define IMGEGL_LOAD_GLES1    2
#define IMGEGL_LOAD_GLES2    3
#define IMGEGL_LOAD_OPENVG   4

/*
 *  Funtion pointer typedef
 */
typedef void (IMG_CALLCONV *IMG_GL_PROC)(IMG_VOID);

IMG_INTERNAL IMG_GL_PROC IMGGetDummyFunction(const char *procname);
IMG_INTERNAL IMG_UINT32 IMGFindLibraryToLoad(const char *procname);

#endif /* _FUNCTION_TABLE_H_ */
