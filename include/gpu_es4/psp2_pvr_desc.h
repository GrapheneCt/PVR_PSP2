#ifndef SCE_GXM_INTERNAL_PVR_DESC_H
#define SCE_GXM_INTERNAL_PVR_DESC_H

// Contains GPU description for PSP2

#define SGX543
#define SGX_FEATURE_MP
#define SGX_FEATURE_MP_CORE_COUNT 4
#define SGX_FEATURE_SYSTEM_CACHE
#define SGX_FEATURE_2D_HARDWARE
#define SUPPORT_HW_RECOVERY
#define PVR_SECURE_HANDLES
#define SGX_FAST_DPM_INIT
#define SUPPORT_PERCONTEXT_PB
#define SUPPORT_SGX_PRIORITY_SCHEDULING
#define SUPPORT_SID_INTERFACE
#define SUPPORT_MEMORY_TILING
#define TRANSFER_QUEUE
#define SGX_CORE_REV 113 //Actual revision on Vita is 117, but there is no support for it in this driver version

#endif