/*****************************************************************************
 Name           : execpred.h
 
 Title          : Execution Predicate Instructions Generation interface.
 
 C Author       : Rana-Iftikhar Ahmad
 
 Created        : 27/04/2011
 
 Copyright      : 2002-2011 by Imagination Technologies Ltd.
                  All rights reserved. No part of this software, either
                  material or conceptual may be copied or distributed,
                  transmitted, transcribed, stored in a retrieval system or
                  translated into any human or computer language in any form
                  by any means, electronic, mechanical, manual or otherwise,
                  or disclosed to third parties without the express written
                  permission of Imagination Technologies Ltd,
                  Home Park Estate, Kings Langley, Hertfordshire,
                  WD4 8LZ, U.K.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.5 $

 Modifications  :

 $Log: execpred.h $ 
 *****************************************************************************/
#ifndef __USC_EXECPRED_H
#define __USC_EXECPRED_H
#include "uscshrd.h"
#if defined(EXECPRED)
IMG_VOID ChangeProgramStructToExecPred(PINTERMEDIATE_STATE psState);
#endif /* #if defined(EXECPRED) */
#endif /* #ifndef __USC_EXECPRED_H */

/******************************************************************************
 End of file (execpred.h)
******************************************************************************/
