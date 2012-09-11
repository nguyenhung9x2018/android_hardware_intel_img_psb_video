# Copyright (c) 2011 Intel Corporation. All Rights Reserved.
#
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sub license, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice (including the
# next paragraph) shall be included in all copies or substantial portions
# of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
# IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
# ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

ifeq ($(ENABLE_IMG_GRAPHICS),true)
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

PSBVIDEO_LOG_ENABLE := true

LOCAL_SRC_FILES :=		\
    lnc_cmdbuf.c            \
    object_heap.c           \
    psb_buffer.c            \
    psb_buffer_dm.c         \
    psb_cmdbuf.c            \
    psb_deblock.c           \
    psb_drv_video.c         \
    psb_drv_debug.c         \
    psb_surface_attrib.c    \
    psb_output.c		\
    psb_texture.c            \
    android/psb_output_android.c            \
    android/psb_HDMIExtMode.c               \
    android/psb_android_glue.cpp            \
    android/psb_surface_gralloc.c         \
    android/psb_gralloc.cpp            \
    psb_surface.c           \
    psb_overlay.c		\
    psb_ws_driver.c         \
    vc1_idx.c           	\
    vc1_vlc.c		\
    pnw_H263ES.c		\
    pnw_H264.c		\
    pnw_H264ES.c		\
    pnw_MPEG2.c		\
    pnw_MPEG4.c		\
    pnw_MPEG4ES.c		\
    pnw_VC1.c           \
    pnw_cmdbuf.c		\
    pnw_hostcode.c		\
    pnw_hostheader.c	\
    pnw_hostjpeg.c		\
    pnw_jpeg.c		\
    pnw_rotate.c	\
    tng_vld_dec.c

LOCAL_CFLAGS := -DLINUX -DANDROID -g -Wall -Wno-unused

LOCAL_C_INCLUDES :=			\
    $(TOPDIR)hardware/libhardware/include/hardware         \
    $(TOPDIR)hardware/intel/include         \
    $(TOPDIR)hardware/intel/include/eurasia/pvr2d              \
    $(TOPDIR)framework/base/include                          \
    $(TARGET_OUT_HEADERS)/libva	\
    $(TOPDIR)hardware/intel/linux-2.6/include/drm     \
    $(TARGET_OUT_HEADERS)/libttm	\
    $(TARGET_OUT_HEADERS)/libwsbm	\
    $(TARGET_OUT_HEADERS)/libdrm\
    $(TARGET_OUT_HEADERS)/opengles  \
    $(LOCAL_PATH)/hwdefs \

LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := pvr_drv_video

LOCAL_SHARED_LIBRARIES := libdl libdrm libwsbm libpvr2d libcutils \
                libui libutils libbinder libhardware

ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
LOCAL_CFLAGS += -DTARGET_HAS_MULTIPLE_DISPLAY
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/display
LOCAL_SHARED_LIBRARIES += libmultidisplay
endif

ifeq ($(strip $(PSBVIDEO_LOG_ENABLE)),true)
LOCAL_CFLAGS += -DPSBVIDEO_LOG_ENABLE
LOCAL_SHARED_LIBRARIES += liblog
endif

# Add source codes for Merrifield
MERRIFIELD_PRODUCT := \
	mrfl_vp \
	mrfl_hvp \
	mrfl_sle
ifneq ($(filter $(TARGET_PRODUCT),$(MERRIFIELD_PRODUCT)),)
LOCAL_SRC_FILES += \
    tng_VP8.c \
    tng_jpegdec.c \
    tng_cmdbuf.c tng_hostheader.c tng_hostcode.c tng_picmgmt.c tng_hostbias.c \
    tng_H264ES.c tng_H263ES.c tng_MPEG4ES.c tng_jpegES.c tng_slotorder.c \
LOCAL_SRC_FILES += \
    vsp_VPP.c \
    vsp_cmdbuf.c
LOCAL_CFLAGS += -DPSBVIDEO_MRFL_VPP
LOCAL_CFLAGS += -DPSBVIDEO_MRFL_DEC 
#LOCAL_CFLAGS += -DPSBVIDEO_MSVDX_DEC_TILING
LOCAL_CFLAGS += -DPSBVIDEO_MSVDX_EC
LOCAL_CFLAGS += -DPSBVIDEO_MRFL
else
LOCAL_CFLAGS += -DPSBVIDEO_MFLD
endif

include $(BUILD_SHARED_LIBRARY)
endif

