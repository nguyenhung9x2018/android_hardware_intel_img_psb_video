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


/*
 * Authors:
 *    Fei Jiang <fei.jiang@intel.com>
 *
 */

#include <va/va.h>
#include <va/va_backend.h>

#ifdef ANDROID
#include "android/psb_android_glue.h"
#endif

#include "psb_drv_video.h"
#include "psb_surface.h"
#include "psb_texstreaming.h"

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
#ifdef ANDROID
    driver_data->bcd_ioctrl_num = IS_MFLD(driver_data) ? 0x32 : 0x2c;
#else
    driver_data->bcd_ioctrl_num = 0x32;
#endif

    psb__information_message("In psb_register_video_bcd, call BC_Video_ioctl_request_buffers to request buffers in BCD driver.\n");
    psb__information_message("buffer count is %d, width is %d, stride is %d, height is %d.\n", num_surfaces, width, stride, height);
    ioctl_package.ioctl_cmd = BC_Video_ioctl_request_buffers;
    ioctl_package.inputparam = (int)(&buf_param);
    if (drmCommandWriteRead(driver_data->drm_fd, driver_data->bcd_ioctrl_num, &ioctl_package, sizeof(ioctl_package)) != 0) {
        psb__error_message("Failed to request buffers from buffer class video driver.\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }
    driver_data->bcd_id = ioctl_package.outputparam;
    driver_data->bcd_registered = 1;
    psb__information_message("In psb_register_video_bcd, the allocated bc device id is %d.\n", driver_data->bcd_id);
    psb__information_message("In psb_register_video_bcd, call BC_Video_ioctl_get_buffer_count to get buffer count.\n");
    ioctl_package.ioctl_cmd = BC_Video_ioctl_get_buffer_count;
    ioctl_package.device_id = driver_data->bcd_id;
    if (drmCommandWriteRead(driver_data->drm_fd, driver_data->bcd_ioctrl_num, &ioctl_package, sizeof(ioctl_package)) != 0) {
        psb__error_message("Failed to get buffer count from buffer class video driver.\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    if (ioctl_package.outputparam != num_surfaces) {
        psb__error_message("buffer count is not correct.\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }
    driver_data->bcd_buffer_num = num_surfaces;
    psb__information_message("In psb_register_video_bcd, call BC_Video_ioctl_set_buffer_phyaddr to bind buffer id with physical address.\n");
    bc_buf_ptr_t buf_pa;
    
    driver_data->bcd_ttm_handles = (uint32_t *)calloc(num_surfaces, sizeof(uint32_t));
    if (NULL == driver_data->bcd_ttm_handles) {
        psb__error_message("Out of memory.\n");
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
    for (i = 0; i < num_surfaces; i++) {
        psb_surface_p psb_surface;
        object_surface_p obj_surface = SURFACE(surface_list[i]);
        psb_surface = obj_surface->psb_surface;
        psb_surface->bc_buffer = (i & 0xffff) | ((driver_data->bcd_id & 0xffff) << 16);

        /*get ttm buffer handle*/
        buf_pa.handle = wsbmKBufHandle(wsbmKBuf(psb_surface->buf.drm_buf));
        buf_pa.index = i;
        driver_data->bcd_ttm_handles[buf_pa.index] = buf_pa.handle;
        ioctl_package.ioctl_cmd = BC_Video_ioctl_set_buffer_phyaddr;
        ioctl_package.device_id = driver_data->bcd_id;
        ioctl_package.inputparam = (int)(&buf_pa);
        /*bind bcd buffer index with ttm buffer handle and set buffer phyaddr in kernel driver*/
        if (drmCommandWriteRead(driver_data->drm_fd, driver_data->bcd_ioctrl_num, &ioctl_package, sizeof(ioctl_package)) != 0) {
            psb__error_message("Failed to set buffer phyaddr from buffer class video driver.\n");
            return VA_STATUS_ERROR_UNKNOWN;
        }
    }
    psb__information_message("num_surface = %d, bcd_id = %d\n", num_surfaces, driver_data->bcd_id);
    psb__android_message("In psb_register_video_bcd, num_surface = %d, bcd_id = %d\n", num_surfaces, driver_data->bcd_id);
    return vaStatus;
}

VAStatus psb_release_video_bcd(VADriverContextP ctx)
{
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    /*destroyTextureStreamSource can be called by LayerBuffer::unregisterBuffers automatically
     *But h264vld will not that destroyTextureStreamSource, so still keep it here
     *otherwise, will get "Erroneous page count" error.
     */
#ifdef ANDROID
    psb__information_message("In psb_release_video_bcd, call psb_android_texture_streaming_destroy to destroy texture streaming source.\n");
    if (driver_data->output_method == PSB_PUTSURFACE_SUPSRC)
        psb_android_dynamic_source_destroy();
    else 
        psb_android_texture_streaming_destroy();
#endif
    BC_Video_ioctl_package ioctl_package;
    psb__information_message("In psb_release_video_bcd, call BC_Video_ioctl_release_buffer_device to release video buffer device id.\n");
    ioctl_package.ioctl_cmd = BC_Video_ioctl_release_buffer_device;
    ioctl_package.device_id = driver_data->bcd_id;
    if (drmCommandWriteRead(driver_data->drm_fd, driver_data->bcd_ioctrl_num, &ioctl_package, sizeof(ioctl_package)) != 0) {
        psb__error_message("Failed to release video buffer class device.\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }
    driver_data->bcd_registered = 0;
    driver_data->bcd_buffer_num = 0;
    free(driver_data->bcd_ttm_handles);
    driver_data->bcd_ttm_handles = NULL;

    return vaStatus;
}
