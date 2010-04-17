LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

#PSBVIDEO_LOG_ENABLE := true

LOCAL_SRC_FILES :=		\
	lnc_H263ES.c            \
	lnc_H264ES.c            \
	lnc_MPEG4ES.c           \
	lnc_cmdbuf.c            \
	lnc_hostcode.c          \
	lnc_hostheader.c        \
	lnc_ospm_event.c	\
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
	psb_output.c            \
	psb_overlay.c           \
	psb_surface.c           \
	psb_ws_driver.c         \
	psb_xvva.c          	\
	vc1_idx.c           	\
	vc1_vlc.c

LOCAL_OBJ_FILES :=              \
        lnc_H263ES.o            \
        lnc_H264ES.o            \
        lnc_MPEG4ES.o           \
        lnc_cmdbuf.o            \
        lnc_hostcode.o          \
        lnc_hostheader.o        \
        lnc_ospm_event.o        \
        object_heap.o           \
        psb_H264.o              \
        psb_MPEG2.o             \
        psb_MPEG2MC.o           \
        psb_MPEG4.o             \
        psb_VC1.o               \
        psb_buffer.o            \
        psb_buffer_dm.o         \
        psb_cmdbuf.o            \
        psb_deblock.o           \
        psb_drv_video.o         \
        psb_output.o            \
        psb_overlay.o           \
        psb_surface.o           \
        psb_ws_driver.o         \
        psb_xvva.o              \
        vc1_idx.o               \
        vc1_vlc.o

LOCAL_CFLAGS := -DLINUX -DHAVE_CONFIG_H

LOCAL_C_INCLUDES :=			\
	$(TOPDIR)kernel/include		\
	$(TOPDIR)platform/hardware/intel/pvr/eurasia/pvr2d		\
	$(TARGET_OUT_HEADERS)/libva	\
	$(TOPDIR)kernel/include/drm	\
	$(TARGET_OUT_HEADERS)/libttm	\
	$(TARGET_OUT_HEADERS)/libmemrar	\
	$(TARGET_OUT_HEADERS)/libwsbm	\
	$(TARGET_OUT_HEADERS)/libpsb_drm\
	$(TARGET_OUT_HEADERS)/opengles  \
	$(LOCAL_PATH)/hwdefs

.c.o:
        $(CC) -c $(INCLUDES) $(LOCAL_CFLAGS) $< -o $@

LOCAL_MODULE := psb_drv_video

LOCAL_SHARED_LIBRARIES := libdrm libwsbm libva libmemrar libpvr2d libcutils

ifeq ($(strip $(PSBVIDEO_LOG_ENABLE)),true)
LOCAL_CFLAGS += -DPSBVIDEO_LOG_ENABLE
LOCAL_SHARED_LIBRARIES += liblog
endif

#include $(BUILD_SHARED_LIBRARY)

