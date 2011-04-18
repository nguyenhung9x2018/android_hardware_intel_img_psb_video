LOCAL_PATH := $(call my-dir)

# For msvdx_bin
# =====================================================
include $(CLEAR_VARS)
LOCAL_SRC_FILES := msvdx_bin.c thread0_ss_bin.c thread0_bin.c thread1_bin.c thread2_bin.c thread3_bin.c
 
LOCAL_CFLAGS += -DFRAME_SWITCHING_VARIANT=1 -DSLICE_SWITCHING_VARIANT=1

LOCAL_C_INCLUDES :=

LOCAL_SHARED_LIBRARIES :=

LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := msvdx_bin

include $(BUILD_EXECUTABLE)

# For topaz_bin
# =====================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := topaz_bin.c H263Firmware_bin.c H263FirmwareCBR_bin.c H263FirmwareVBR_bin.c H264Firmware_bin.c H264FirmwareCBR_bin.c H264FirmwareVBR_bin.c MPG4Firmware_bin.c MPG4FirmwareCBR_bin.c MPG4FirmwareVBR_bin.c H264FirmwareVCM_bin.c

LOCAL_CFLAGS +=

LOCAL_C_INCLUDES :=

LOCAL_SHARED_LIBRARIES :=

LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := topaz_bin

include $(BUILD_EXECUTABLE)

# For fwinfo
# =====================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := fwinfo.c

LOCAL_CFLAGS +=

LOCAL_C_INCLUDES :=

LOCAL_SHARED_LIBRARIES :=

LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := imginfo

include $(BUILD_EXECUTABLE)
