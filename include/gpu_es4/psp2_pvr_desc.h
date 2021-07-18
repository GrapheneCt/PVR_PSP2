#ifndef PSP2_PVR_DESC_H
#define PSP2_PVR_DESC_H

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
#define SUPPORT_SGX_NEW_STATUS_VALS
#define SUPPORT_SID_INTERFACE
#define SUPPORT_MEMORY_TILING
#define TRANSFER_QUEUE
#define SGX_CORE_REV 113 //Actual revision on Vita is 117, but there is no support for it in this driver version

// Defined here due to a bug in core revision 113 which is seemingly not present in revision 117.
#define EUR_CR_PDS_PP_INDEPENDANT_STATE     0x0AE8
#define EUR_CR_PDS_PP_INDEPENDANT_STATE_DISABLE_MASK 0x00000001U
#define EUR_CR_PDS_PP_INDEPENDANT_STATE_DISABLE_SHIFT 0
#define EUR_CR_PDS_PP_INDEPENDANT_STATE_DISABLE_SIGNED 0

#endif