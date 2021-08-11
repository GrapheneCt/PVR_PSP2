# Copyright	2010 Imagination Technologies Limited. All rights reserved.
#
# No part of this software, either material or conceptual may be
# copied or distributed, transmitted, transcribed, stored in a
# retrieval system or translated into any human or computer
# language in any form by any means, electronic, mechanical,
# manual or other-wise, or disclosed to third parties without
# the express written permission of: Imagination Technologies
# Limited, HomePark Industrial Estate, Kings Langley,
# Hertfordshire, WD4 8LZ, UK
#
# $Log: Linux.mk $
#

modules := opengles2

opengles2_type := shared_library

ifeq ($(SUPPORT_ANDROID_PLATFORM),1)
opengles2_target := libGLESv2_POWERVR_SGX$(SGXCORE)_$(SGX_CORE_REV).so
else
opengles2_target := libGLESv2.so
endif

opengles2_src = \
 accum.c \
 binshader.c \
 bufobj.c \
 clear.c \
 drawvarray.c \
 eglglue.c \
 eglimage.c \
 fbo.c \
 get.c \
 gles2errata.c \
 makemips.c \
 metrics.c \
 misc.c \
 names.c \
 pdump.c \
 pixelop.c \
 profile.c \
 shader.c \
 scissor.c \
 sgxif.c \
 spanpack.c \
 state.c \
 statehash.c \
 tex.c \
 texdata.c \
 texformat.c \
 texmgmt.c \
 texstream.c \
 texyuv.c \
 uniform.c \
 use.c \
 validate.c \
 vertex.c \
 vertexarrobj.c \
 $(TOP)/common/tls/linux_tls.c \
 $(TOP)/common/tls/nothreads_tls.c

opengles2_obj := \
 $(call target-intermediates-of,dmscalc,dmscalc.o) \
 $(call target-intermediates-of,common,buffers.o codeheap.o kickresource.o twiddle.o) \
 $(call target-intermediates-of,pixfmts,sgxpixfmts.o) \
 $(call target-intermediates-of,pixevent,pixevent.o pixeventpbesetup.o) \
 $(call target-intermediates-of,pdsogles,pds.o) \
 $(call target-intermediates-of,usegen,usegen.o) \
 $(call target-intermediates-of,combiner,combiner.o)

opengles2_cflags := \
 -DOGLES2_MODULE -DPDS_BUILD_OPENGLES -DOUTPUT_USPBIN \
 -DOPTIMISE_NON_NPTL_SINGLE_THREAD_TLS_LOOKUP

opengles2_includes := include4 hwdefs eurasiacon/include common/tls \
 common/dmscalc common/combiner eurasiacon/common \
 codegen/pds codegen/pixevent codegen/pixfmts codegen/usegen \
 codegen/combiner \
 tools/intern/usp tools/intern/usc2 \
 tools/intern/oglcompiler/glsl tools/intern/oglcompiler/powervr \
 tools/intern/oglcompiler/binshader \
 tools/intern/useasm \
 $(TARGET_INTERMEDIATES)/pds_mte_state_copy \
 $(TARGET_INTERMEDIATES)/pds_aux_vtx

opengles2_libs := srv_um_SGX$(SGXCORE)_$(SGX_CORE_REV) IMGegl_SGX$(SGXCORE)_$(SGX_CORE_REV)
opengles2_staticlibs := usp
opengles2_extlibs := m pthread

ifeq ($(BUILD),timing)
# FIXME: Support TIMING_LEVEL>1
opengles2_cflags += -DTIMING_LEVEL=1
else
opengles2_cflags += -DTIMING_LEVEL=0
endif

ifeq ($(BUILD),debug)
opengles2_staticlibs += useasm
endif

ifeq ($(EGL_EXTENSION_ANDROID_BLOB_CACHE),1)
 opengles2_src += digest.c
 ifeq ($(SUPPORT_ANDROID_PLATFORM),1)
  opengles2_extlibs += crypto
 else
$(patsubst %.c,$(TARGET_INTERMEDIATES)/opengles2/%.o,$(opengles2_src)): \
 $(TARGET_OUT)/.$(openssl)
  opengles2_packages := openssl
  opengles2_ldflags := -Wl,--version-script,$(THIS_DIR)/crypto.lds
 endif
endif

$(addprefix $(TARGET_INTERMEDIATES)/opengles2/,eglglue.o validate.o): \
 $(call target-intermediates-of,pds_mte_state_copy,pds_mte_state_copy.h pds_mte_state_copy_sizeof.h) \
 $(call target-intermediates-of,pds_aux_vtx,pds_aux_vtx.h pds_aux_vtx_sizeof.h)
