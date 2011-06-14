ifeq ($(ENABLE_IMG_GRAPHICS),true)
# INTEL CONFIDENTIAL
# Copyright 2007 Intel Corporation. All Rights Reserved.
#
# The source code contained or described herein and all documents related to
# the source code ("Material") are owned by Intel Corporation or its suppliers
# or licensors. Title to the Material remains with Intel Corporation or its
# suppliers and licensors. The Material may contain trade secrets and
# proprietary and confidential information of Intel Corporation and its
# suppliers and licensors, and is protected by worldwide copyright and trade
# secret laws and treaty provisions. No part of the Material may be used,
# copied, reproduced, modified, published, uploaded, posted, transmitted,
# distributed, or disclosed in any way without Intel's prior express written
# permission. 
# 
# No license under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or delivery
# of the Materials, either expressly, by implication, inducement, estoppel or
# otherwise. Any license under such intellectual property rights must be
# express and approved by Intel in writing.
#
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

PSBVIDEO_LOG_ENABLE := true

LOCAL_SRC_FILES :=		\
    lnc_H263ES.c            \
    lnc_H264ES.c            \
    lnc_MPEG4ES.c           \
    lnc_cmdbuf.c            \
    lnc_hostcode.c          \
    lnc_hostheader.c        \
    lnc_ospm.c		\
    object_heap.c           \
    psb_H264.c          	\
    psb_MPEG2.c         	\
    psb_MPEG2MC.c           \
    psb_MPEG4.c         	\
    psb_VC1.c           	\
    psb_buffer.c            \
    psb_buffer_dm.c         \
    psb_cmdbuf.c            \
    psb_deblock.c           \
    psb_drv_video.c         \
    psb_output.c		\
    psb_texstreaming.c            \
    psb_texture.c            \
    android/psb_output_android.c            \
    android/psb_HDMIExtMode.c               \
    android/psb_android_glue.cpp            \
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
    pnw_rotate.c		\
    powervr_iep_lite/csc/csc2.c \
    powervr_iep_lite/csc/csc2_data.c \
    powervr_iep_lite/fixedpointmaths/fixedpointmaths.c \
    powervr_iep_lite/iep_lite/iep_lite_api.c \
    powervr_iep_lite/iep_lite/iep_lite_hardware.c \
    powervr_iep_lite/iep_lite/iep_lite_utils.c

LOCAL_CFLAGS := -DLINUX -DANDROID -g -Wall -Wno-unused

LOCAL_C_INCLUDES :=			\
    $(TOPDIR)hardware/intel/include         \
    $(TOPDIR)hardware/intel/include/eurasia/pvr2d              \
    $(TARGET_OUT_HEADERS)/libva	\
    $(TOPDIR)hardware/intel/include/drm     \
    $(TARGET_OUT_HEADERS)/libttm	\
    $(TARGET_OUT_HEADERS)/libmemrar	\
    $(TARGET_OUT_HEADERS)/libwsbm	\
    $(TARGET_OUT_HEADERS)/libpsb_drm\
    $(TARGET_OUT_HEADERS)/opengles  \
    $(LOCAL_PATH)/hwdefs		\
    $(LOCAL_PATH)/powervr_iep_lite/include		\
    $(LOCAL_PATH)/powervr_iep_lite/include/win32	\
    $(LOCAL_PATH)/powervr_iep_lite/csc		\
    $(LOCAL_PATH)/powervr_iep_lite/iep_lite		\
    $(LOCAL_PATH)/powervr_iep_lite/fixedpointmaths

LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := pvr_drv_video

LOCAL_SHARED_LIBRARIES := libdl libdrm libwsbm libmemrar libpvr2d libcutils \
                libui libutils libbinder libsurfaceflinger_client

ifeq ($(strip $(PSBVIDEO_LOG_ENABLE)),true)
LOCAL_CFLAGS += -DPSBVIDEO_LOG_ENABLE
LOCAL_SHARED_LIBRARIES += liblog
endif

include $(BUILD_SHARED_LIBRARY)
endif

