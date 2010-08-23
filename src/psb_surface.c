/*
 * INTEL CONFIDENTIAL
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

#include <wsbm/wsbm_manager.h>

#include "psb_def.h"
#include "psb_surface.h"

#include <RAR/rar.h>


/*
 * Create surface
 */
VAStatus psb_surface_create( psb_driver_data_p driver_data,
                             int width, int height, int fourcc, int protected,
                             psb_surface_p psb_surface /* out */
                             )
{
    int ret;

    if (fourcc == VA_FOURCC_NV12) 
    {
        if ((width <= 0) || (width > 5120) || (height <= 0) || (height > 5120))
        {
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }
	    
        if (0)
        {
            ;
        }
        else if (512 >= width)
        {
            psb_surface->stride_mode = STRIDE_512;
            psb_surface->stride = 512;
        }
        else if (1024 >= width)
        {
            psb_surface->stride_mode = STRIDE_1024;
            psb_surface->stride = 1024;
        }
        else if (1280 >= width)
        {
            psb_surface->stride_mode = STRIDE_1280;
            psb_surface->stride = 1280;
        }
        else if (2048 >= width)
        {
            psb_surface->stride_mode = STRIDE_2048;
            psb_surface->stride = 2048;
        }
        else if (4096 >= width)
        {
            psb_surface->stride_mode = STRIDE_4096;
            psb_surface->stride = 4096;
        }
        else
        {
            psb_surface->stride_mode = STRIDE_NA;
            psb_surface->stride = (width + 0x1f) & ~0x1f;
        }
	    
        psb_surface->luma_offset = 0;
        psb_surface->chroma_offset = psb_surface->stride * height;
        psb_surface->size = (psb_surface->stride * height * 3) / 2;
        psb_surface->extra_info[4] = VA_FOURCC_NV12;
    }
    else if (fourcc == VA_FOURCC_RGBA)  
    {
        unsigned int pitchAlignMask = 63;
        psb_surface->stride_mode = STRIDE_NA;
        psb_surface->stride = (width * 4 + pitchAlignMask) & ~pitchAlignMask;;
        psb_surface->luma_offset = 0;
        psb_surface->chroma_offset = 0;
        psb_surface->size = psb_surface->stride * height;
        psb_surface->extra_info[4] = VA_FOURCC_RGBA;
    }
	
    if (protected == 0)
        ret = psb_buffer_create( driver_data, psb_surface->size, psb_bt_surface, &psb_surface->buf );
    else
        ret = psb_buffer_create_rar( driver_data, psb_surface->size, &psb_surface->buf );
        
    return ret ? VA_STATUS_ERROR_ALLOCATION_FAILED : VA_STATUS_SUCCESS;
}

/* id_or_ofs: it is frame ID or frame offset in camear device memory
 *     for CI frame: it it always frame offset currently
 *     for v4l2 buf: it is offset used in V4L2 buffer mmap
 */
VAStatus psb_surface_create_camera( psb_driver_data_p driver_data,
                             int width, int height, int stride, int size,
                             psb_surface_p psb_surface, /* out */
                             int is_v4l2,
                             unsigned int id_or_ofs
                             )
{
    int ret;

    if ((width <= 0) || (width > 4096) || (height <= 0) || (height > 4096))
    {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
    
    psb_surface->stride = stride; 
    psb_surface->chroma_offset = psb_surface->stride * height;
    psb_surface->size = (psb_surface->stride * height * 3) / 2;

    ret = psb_buffer_create_camera(driver_data,&psb_surface->buf,
                                   is_v4l2, id_or_ofs);
    
    if (ret != VA_STATUS_SUCCESS) {
        psb_surface_destroy(psb_surface);
        
        psb__error_message("Get surfae offset of camear device memory failed!\n");
        return ret;
    }

    return VA_STATUS_SUCCESS;
}

/* id_or_ofs: it is frame ID or frame offset in camear device memory
 *     for CI frame: it it always frame offset currently
 *     for v4l2 buf: it is offset used in V4L2 buffer mmap
 * user_ptr: virtual address of user buffer.    
 */
VAStatus psb_surface_create_camera_from_ub( psb_driver_data_p driver_data,
                             int width, int height, int stride, int size,
                             psb_surface_p psb_surface, /* out */
                             int is_v4l2,
                             unsigned int id_or_ofs,
			     const unsigned long *user_ptr)
{
    int ret;

    if ((width <= 0) || (width > 4096) || (height <= 0) || (height > 4096))
    {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
    
    psb_surface->stride = stride; 
    psb_surface->chroma_offset = psb_surface->stride * height;
    psb_surface->size = (psb_surface->stride * height * 3) / 2;

    ret = psb_buffer_create_camera_from_ub(driver_data,&psb_surface->buf,
                                   is_v4l2, psb_surface->size, user_ptr);
    
    if (ret != VA_STATUS_SUCCESS) {
        psb_surface_destroy(psb_surface);
        
        psb__error_message("Get surfae offset of camear device memory failed!\n");
        return ret;
    }

    return VA_STATUS_SUCCESS;
}


/*
 * Temporarily map surface and set all chroma values of surface to 'chroma'
 */   
VAStatus psb_surface_set_chroma( psb_surface_p psb_surface, int chroma )
{
    void *surface_data;
    int ret = psb_buffer_map(&psb_surface->buf, &surface_data);
    
    if (ret) return VA_STATUS_ERROR_UNKNOWN;

    memset(surface_data + psb_surface->chroma_offset, chroma, psb_surface->size - psb_surface->chroma_offset);
    
    psb_buffer_unmap(&psb_surface->buf);

    return VA_STATUS_SUCCESS;
}

/*
 * Destroy surface
 */   
void psb_surface_destroy( psb_surface_p psb_surface )
{
    psb_buffer_destroy( &psb_surface->buf );

    if( NULL != psb_surface->in_loop_buf )
        psb_buffer_destroy( psb_surface->in_loop_buf );
        
}

VAStatus psb_surface_sync( psb_surface_p psb_surface )
{
    wsbmBOWaitIdle(psb_surface->buf.drm_buf, 0);
    
    return VA_STATUS_SUCCESS;
}

VAStatus psb_surface_query_status( psb_surface_p psb_surface, VASurfaceStatus *status )
{
    int ret;
    uint32_t synccpu_flag = WSBM_SYNCCPU_READ | WSBM_SYNCCPU_WRITE | WSBM_SYNCCPU_DONT_BLOCK;
    
    ret = wsbmBOSyncForCpu(psb_surface->buf.drm_buf, synccpu_flag);
    (void) wsbmBOReleaseFromCpu(psb_surface->buf.drm_buf, synccpu_flag);
    
    if (ret == 0)
        *status = VASurfaceReady;
    else
        *status = VASurfaceRendering;
        
    return VA_STATUS_SUCCESS;
}
