/*
  INTEL CONFIDENTIAL
* Copyright 2007 Intel Corporation. All Rights Reserved.
*
* The source code contained or described herein and all documents related to
* the source code ("Material") are owned by Intel Corporation or its suppliers
* or licensors. Title to the Material remains with Intel Corporation or its
* suppliers and licensors. The Material may contain trade secrets and
* proprietary and confidential information of Intel Corporation and its
* suppliers and licensors, and is protected by worldwide copyright and trade
* secret laws and treaty provisions. No part of the Material may be used,
* copied, reproduced, modified, published, uploaded, posted, transmitted,
* distributed, or disclosed in any way without Intel's prior express written
* permission. 
* 
* No license under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or delivery
* of the Materials, either expressly, by implication, inducement, estoppel or
* otherwise. Any license under such intellectual property rights must be
* express and approved by Intel in writing.
*/

#include <va/va.h>
#include <va/va_backend.h>
#include <cutils/log.h>
#include "psb_drv_video.h"
#include "psb_android_glue.h"
#include "psb_surface.h"

#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;
#define SURFACE(id) ((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))

VAStatus psb_register_video_bcd(
    VADriverContextP ctx, 
    int width, 
    int height, 
    int stride, 
    int num_surfaces, 
    VASurfaceID *surface_list
)
{
    INIT_DRIVER_DATA
    int i;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    BC_Video_ioctl_package ioctl_package;
    bc_buf_params_t buf_param;

    buf_param.count = num_surfaces;
    buf_param.width = width;
    buf_param.stride = stride;
    buf_param.height = height;
    buf_param.fourcc = BC_PIX_FMT_NV12;
    buf_param.type = BC_MEMORY_USERPTR;

    LOGD("In psb_register_video_bcd, call BC_Video_ioctl_request_buffers to request buffers in BCD driver.\n");
    LOGD("buffer count is %d, width is %d, stride is %d, height is %d.\n", num_surfaces, width, stride, height);
    ioctl_package.ioctl_cmd = BC_Video_ioctl_request_buffers;
    ioctl_package.inputparam = (int)(&buf_param);
    if(drmCommandWriteRead(driver_data->drm_fd, DRM_BUFFER_CLASS_VIDEO, &ioctl_package, sizeof(ioctl_package)) != 0)
    {
        LOGE("Failed to request buffers from buffer class video driver (errno=%d).\n", errno);
        return VA_STATUS_ERROR_UNKNOWN;
    }
    buffer_device_id = ioctl_package.outputparam;
    LOGD("In psb_register_video_bcd, the allocated bc device id is %d.\n", buffer_device_id);
    LOGD("In psb_register_video_bcd, call BC_Video_ioctl_get_buffer_count to get buffer count.\n");
    ioctl_package.ioctl_cmd = BC_Video_ioctl_get_buffer_count;
    ioctl_package.device_id = buffer_device_id;
    if (drmCommandWriteRead(driver_data->drm_fd, DRM_BUFFER_CLASS_VIDEO, &ioctl_package, sizeof(ioctl_package)) != 0)
    {
        LOGE("Failed to get buffer count from buffer class video driver (errno=%d).\n", errno);
        return VA_STATUS_ERROR_UNKNOWN;
    }

    if (ioctl_package.outputparam != num_surfaces) {
        LOGE("buffer count is not correct (errno=%d).\n", errno);
        return VA_STATUS_ERROR_UNKNOWN;
    }

    LOGD("In psb_register_video_bcd, call BC_Video_ioctl_set_buffer_phyaddr to bind buffer id with physical address.\n");
    bc_buf_ptr_t buf_pa;

    for (i = 0; i < num_surfaces; i++)
    {
        psb_surface_p psb_surface;
        object_surface_p obj_surface = SURFACE(surface_list[i]);
        psb_surface = obj_surface->psb_surface;
        /*get ttm buffer handle*/
        buf_pa.handle = wsbmKBufHandle(wsbmKBuf(psb_surface->buf.drm_buf));
        buf_pa.index = i;
        ioctl_package.ioctl_cmd = BC_Video_ioctl_set_buffer_phyaddr;
        ioctl_package.device_id = buffer_device_id;
        ioctl_package.inputparam = (int) (&buf_pa);
        /*bind bcd buffer index with ttm buffer handle and set buffer phyaddr in kernel driver*/
        if(drmCommandWriteRead(driver_data->drm_fd, DRM_BUFFER_CLASS_VIDEO, &ioctl_package, sizeof(ioctl_package)) != 0)
        {
            LOGE("Failed to set buffer phyaddr from buffer class video driver (errno=%d).\n", errno);
            return VA_STATUS_ERROR_UNKNOWN;
        }
    }
    return vaStatus;
}

VAStatus psb_release_video_bcd( VADriverContextP ctx )
{
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    BC_Video_ioctl_package ioctl_package;

    if (driver_data->output_method == PSB_PUTSURFACE_TEXSTREAMING || 
	driver_data->output_method == PSB_PUTSURFACE_FORCE_TEXSTREAMING) {
        LOGD("In psb_release_video_bcd, call BC_Video_ioctl_release_buffer_device to release video buffer device id.\n");
        ioctl_package.ioctl_cmd = BC_Video_ioctl_release_buffer_device;
        ioctl_package.device_id = buffer_device_id;
        if (drmCommandWriteRead(driver_data->drm_fd, DRM_BUFFER_CLASS_VIDEO, &ioctl_package, sizeof(ioctl_package)) != 0)
        {
            LOGE("Failed to release video buffer class device (errno=%d).\n", errno);
            return VA_STATUS_ERROR_UNKNOWN;
        }
        LOGD("In psb_release_video_bcd, call psb_android_texture_streaming_destroy to destroy texture streaming source.\n");
        psb_android_texture_streaming_destroy();        
    }    
    return vaStatus;
}
