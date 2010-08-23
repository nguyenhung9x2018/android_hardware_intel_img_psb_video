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

#include <va/va_backend.h>
#include "psb_output.h"
#include "psb_surface.h"
#include "psb_buffer.h"
#include "psb_overlay.h"
#include "psb_texture.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <cutils/log.h>
#include "psb_android_glue.h"
#include <wsbm/wsbm_manager.h>

#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;
#define INIT_OUTPUT_PRIV    psb_android_output_p output = (psb_android_output_p)(((psb_driver_data_p)ctx->pDriverData)->ws_priv)

#define SURFACE(id) ((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &driver_data->buffer_heap, id ))
#define IMAGE(id)  ((object_image_p) object_heap_lookup( &driver_data->image_heap, id ))
#define SUBPIC(id)  ((object_subpic_p) object_heap_lookup( &driver_data->subpic_heap, id ))
#define CONTEXT(id) ((object_context_p) object_heap_lookup( &driver_data->context_heap, id ))


void *psb_android_output_init(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    char put_surface[1024];
    psb_android_output_p output = calloc(1, sizeof(psb_android_output_s));
    
    struct fb_var_screeninfo vinfo = {0};
    int fbfd = -1;
    int ret;

    if (output == NULL) {
        psb__error_message("Can't malloc memory\n");
	return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    /* Guess the screen size */
    output->screen_width = 800;
    output->screen_height = 480;

    // Open the frame buffer for reading
    fbfd = open("/dev/graphics/fb0", O_RDONLY);
    if (fbfd) {
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo))
	    psb__information_message("Error reading screen information.\n");
    }
    close(fbfd);
    output->screen_width = vinfo.xres;
    output->screen_height = vinfo.yres;
    
    if (psb_parse_config("PSB_VIDEO_CTEXTURE", &put_surface[0]) == 0) {
        psb__information_message("Putsurface use client textureblit\n");
        driver_data->output_method = PSB_PUTSURFACE_FORCE_CTEXTURE;
        driver_data->ctexture = 1;
    }

    if (psb_parse_config("PSB_VIDEO_COVERLAY", &put_surface[0]) == 0) {
        psb__information_message("Putsurface use client overlay\n");
        driver_data->output_method = PSB_PUTSURFACE_FORCE_COVERLAY;
        driver_data->coverlay = 1;
    }
    
    /* config file not exist or parse failed, use texturestreaming by default*/
    if (!ret || ((driver_data->output_method != PSB_PUTSURFACE_FORCE_CTEXTURE) && (driver_data->output_method != PSB_PUTSURFACE_FORCE_COVERLAY)))
	driver_data->output_method = PSB_PUTSURFACE_TEXSTREAMING;

    if (getenv("PSB_VIDEO_CTEXTURE")) {
	psb__information_message("Putsurface use client textureblit\n");
	driver_data->output_method = PSB_PUTSURFACE_FORCE_CTEXTURE;
	driver_data->ctexture = 1;
    }
    
    if (getenv("PSB_VIDEO_COVERLAY")) {
        psb__information_message("Putsurface use client overlay\n");
        driver_data->output_method = PSB_PUTSURFACE_FORCE_COVERLAY;
	driver_data->coverlay = 1;
    } 
    
    return output;
}

VAStatus psb_android_output_deinit(VADriverContextP ctx)
{
    INIT_OUTPUT_PRIV;
    //psb_android_output_p output = GET_OUTPUT_DATA(ctx);

    return VA_STATUS_SUCCESS;
}

static VAStatus psb_putsurface_ctexture(
    VADriverContextP ctx,
    VASurfaceID surface,
    unsigned char* data,
    short srcx,
    short srcy,
    unsigned short srcw,
    unsigned short srch,
    short destx,
    short desty,
    unsigned short destw,
    unsigned short desth,
    unsigned int flags /* de-interlacing flags */
)
{
    INIT_DRIVER_DATA;
    INIT_OUTPUT_PRIV;
    object_surface_p obj_surface = SURFACE(surface);
    int offset = 0;
    psb_surface_p psb_surface;

    obj_surface = SURFACE(surface);
    psb_surface = obj_surface->psb_surface;

//    psb_surface->buf.drm_buf;
//    psb_surface->buf.pl_flags;

#if 0
    printf("pl_flags %x\n", psb_surface->buf.pl_flags);

    printf("FIXME: not sure how Android app handle rotation?\n"
           "need to revise width & height here?\n");

    printf("FIXME: need to prepare a rotation/RAR surface here?\n");

    printf("FIXME: camera preview surface is different, all is \n"
           "just one buffer, so a pre_add is needed\n");
    LOGE("srcx %d, srcy %d, srcw %d, srch %d, destx %d, desty %d, destw %d,\n"
         "desth %d, width %d height %d, stride %d drm_buf %x\n",
         srcx, srcy, srcw, srch, destx, desty, destw, desth, obj_surface->width,
         obj_surface->height, psb_surface->stride, psb_surface->buf.drm_buf);
#endif
    psb_putsurface_textureblit(ctx, data, srcx, srcy, srcw, srch, destx, desty, destw, desth,
                        obj_surface->width, obj_surface->height,
                        psb_surface->stride, psb_surface->buf.drm_buf,
                        psb_surface->buf.pl_flags);

    psb_android_postBuffer(offset);
}


VAStatus psb_putsurface_coverlay(
    VADriverContextP ctx,
    VASurfaceID surface,
    short srcx,
    short srcy,
    unsigned short srcw,
    unsigned short srch,
    short destx, /* screen cooridination */
    short desty,
    unsigned short destw,
    unsigned short desth,
    unsigned int flags /* de-interlacing flags */
)
{
    INIT_OUTPUT_PRIV;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    
    /* USE_FIT_SCR_SIZE */
    /* calculate fit screen size of frame */
    unsigned short _scr_x = output->screen_width;
    unsigned short _scr_y = output->screen_height;
    float _slope_xy = (float)srch/srcw;
    unsigned short _destw = (short)(_scr_y/_slope_xy);
    unsigned short _desth = (short)(_scr_x*_slope_xy);
    short _pos_x, _pos_y;
    
    if (_destw <= _scr_x) {
        _desth = _scr_y;
        _pos_x = (_scr_x-_destw)>>1;
        _pos_y = 0;
    } else {
        _destw = _scr_x;
        _pos_x = 0;
        _pos_y = (_scr_y-_desth)>>1;
    }
    destx += _pos_x;
    desty += _pos_y;
    destw = _destw;
    desth = _desth;

    /* display by overlay */
    vaStatus = psb_putsurface_overlay(
        ctx, surface, srcx, srcy, srcw, srch,
        destx, desty, destw, desth, /* screen coordinate */
        flags); 

    return vaStatus;
}


VAStatus psb_PutSurface(
    VADriverContextP ctx,
    VASurfaceID surface,
    void *android_isurface,
    short srcx,
    short srcy,
    unsigned short srcw,
    unsigned short srch,
    short destx,
    short desty,
    unsigned short destw,
    unsigned short desth,
    VARectangle *cliprects, /* client supplied clip list */
    unsigned int number_cliprects, /* number of clip rects in the clip list */
    unsigned int flags /* de-interlacing flags */
)
{
    INIT_DRIVER_DATA;
    INIT_OUTPUT_PRIV;
    object_surface_p obj_surface;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    obj_surface = SURFACE(surface);
    if (NULL == obj_surface)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    if (driver_data->dummy_putsurface) {
        psb__information_message("vaPutSurface: dummy mode, return directly\n");
        return VA_STATUS_SUCCESS;
    }

    if (driver_data->output_method == PSB_PUTSURFACE_TEXSTREAMING || 
	driver_data->output_method == PSB_PUTSURFACE_FORCE_TEXSTREAMING) {
        LOGV("In psb_PutSurface, use texture streaming to display video.\n");
        if (!output->register_flag)
        {
            LOGD("In psb_PutSurface, call psb_android_register_isurface to create texture streaming source, srcw is %d, srch is %d.\n", srcw, srch);
            psb_android_register_isurface(android_isurface, buffer_device_id, srcw, srch);
            output->register_flag = 1;
        }

        BC_Video_ioctl_package ioctl_package;
        psb_surface_p psb_surface;
        psb_surface = obj_surface->psb_surface;
        ioctl_package.ioctl_cmd = BC_Video_ioctl_get_buffer_index;
        ioctl_package.device_id = buffer_device_id;
        ioctl_package.inputparam = (int) (wsbmKBufHandle(wsbmKBuf(psb_surface->buf.drm_buf)));

        if(drmCommandWriteRead(driver_data->drm_fd, DRM_BUFFER_CLASS_VIDEO, &ioctl_package, sizeof(ioctl_package)) != 0)
        {
            LOGE("Failed to get buffer index from buffer class video driver (errno=%d).\n", errno);
            return VA_STATUS_ERROR_UNKNOWN;
        }
        LOGV("buffer handle is %d and buffer index is %d.\n", ioctl_package.inputparam, ioctl_package.outputparam);
        psb_android_texture_streaming_display(ioctl_package.outputparam);
        
        return VA_STATUS_SUCCESS;
    } else if (driver_data->output_method == PSB_PUTSURFACE_CTEXTURE || 
               driver_data->output_method == PSB_PUTSURFACE_FORCE_CTEXTURE) {
        LOGD("In psb_PutSurface, use general texture path to display video.\n");
        /* register Buffer only once */
    	if (!output->register_flag) {
            LOGV("In psb_PutSurface, call psb_android_register_isurface to create texture streaming source.\n");
    	    output->heap_addr = psb_android_registerBuffers(android_isurface, getpid(), srcw, srch);
    	    if (VA_STATUS_SUCCESS != vaStatus) {
    		    psb__information_message("vaPutSurface: register buffers failed, return directly\n");
    		    return vaStatus;
    	    }
    	    output->register_flag = 1;
	    }

	    return psb_putsurface_ctexture(ctx, surface, output->heap_addr, srcx, srcy, srcw, srch,
                                       destx, desty, destw, desth, flags);
    } else if (driver_data->output_method == PSB_PUTSURFACE_COVERLAY ||
               driver_data->output_method == PSB_PUTSURFACE_FORCE_COVERLAY) {
        LOGV("In psb_PutSurface, use overlay to display video.\n");
        LOGV("srcx is %d, srcy is %d, srcw is %d, srch is %d, destx is %d, desty is %d, destw is %d, desth is %d.\n", \
             srcx, srcy, srcw, srch, destx, desty, destw, desth);
        return psb_putsurface_coverlay(ctx, surface,
                                      srcx, srcy, srcw, srch,
                                      destx, desty, destw, desth,
				      flags);
    }
}
