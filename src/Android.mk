ifeq ($(strip $(ENABLE_IMG_GRAPHICS)),true)

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
	psb_texture.c		\
	psb_VC1.c           	\
	psb_buffer.c            \
	psb_buffer_dm.c         \
	psb_cmdbuf.c            \
	psb_deblock.c           \
	psb_drv_video.c         \
	psb_output.c		\
	android/psb_output_android.c            \
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
	pnw_cmdbuf.c		\
	pnw_hostcode.c		\
	pnw_hostheader.c	\
	pnw_hostjpeg.c		\
	pnw_jpeg.c

LOCAL_CFLAGS := -DLINUX -DANDROID -g -Wall -Wno-unused

LOCAL_C_INCLUDES :=			\
	$(TOPDIR)hardware/intel/include		\
        $(TOPDIR)hardware/intel/include/eurasia/pvr2d              \
	$(TARGET_OUT_HEADERS)/libva	\
	$(TOPDIR)hardware/intel/include/drm	\
	$(TARGET_OUT_HEADERS)/libttm	\
	$(TARGET_OUT_HEADERS)/libmemrar	\
	$(TARGET_OUT_HEADERS)/libwsbm	\
	$(TARGET_OUT_HEADERS)/libpsb_drm\
	$(TARGET_OUT_HEADERS)/opengles  \
	$(LOCAL_PATH)/hwdefs

LOCAL_MODULE := pvr_drv_video

LOCAL_CXX := g++

LOCAL_SHARED_LIBRARIES := libdl libdrm libwsbm libmemrar libpvr2d libcutils \
			  libui libutils libbinder

ifeq ($(strip $(PSBVIDEO_LOG_ENABLE)),true)
LOCAL_CFLAGS += -DPSBVIDEO_LOG_ENABLE
LOCAL_SHARED_LIBRARIES += liblog
endif

include $(BUILD_SHARED_LIBRARY)
endif
