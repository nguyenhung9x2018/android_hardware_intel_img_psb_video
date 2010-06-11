/*
 * Copyright (c) 2007 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
#include "psb_android_glue.h"
#include <wsbm/wsbm_manager.h>

#define INIT_DRIVER_DATA	psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;

#define SURFACE(id)	((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &driver_data->buffer_heap, id ))
#define IMAGE(id)  ((object_image_p) object_heap_lookup( &driver_data->image_heap, id ))
#define SUBPIC(id)  ((object_subpic_p) object_heap_lookup( &driver_data->subpic_heap, id ))
#define CONTEXT(id) ((object_context_p) object_heap_lookup( &driver_data->context_heap, id ))

VAStatus psb_init_xvideo(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    psb_output_p output = GET_OUTPUT_DATA(ctx);

    if (getenv("PSB_VIDEO_TEXTURE_XV"))
    {
	output->register_flag = 0;
	output->heap_addr = NULL;
	psb__information_message("Putsurface use Texture video by force\n");
	output->output_method = PSB_PUTSURFACE_FORCE_TEXTURE_XV;
	init_test_texture(ctx);
    }
    else
    {
	if (getenv("PSB_VIDEO_OVERLAY_XV"))
	{
	    psb__information_message("Putsurface use Overlay video by force\n");
	    output->output_method = PSB_PUTSURFACE_FORCE_OVERLAY_XV;
	}
	else
	{
	    psb__information_message("Putsurface use Overlay video by default\n");
	}

	psbInitVideo(ctx);
    }
	
    return VA_STATUS_SUCCESS;
}

VAStatus psb_deinit_xvideo(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    psb_output_p output = GET_OUTPUT_DATA(ctx);

    if (output->output_method == PSB_PUTSURFACE_TEXTURE_XV || output->output_method ==  PSB_PUTSURFACE_FORCE_TEXTURE_XV)
    {
	output->register_flag = 0;
	output->heap_addr = NULL;
	deinit_test_texture(ctx);
	psb_android_clearHeap();
    }

    if (output->output_method == PSB_PUTSURFACE_OVERLAY_XV || output->output_method == PSB_PUTSURFACE_FORCE_OVERLAY_XV)
	psbDeInitVideo(ctx);

    return VA_STATUS_SUCCESS;
}

static uint32_t mask2shift(uint32_t mask)
{
    uint32_t shift = 0;
    while((mask & 0x1) == 0)
    {
        mask = mask >> 1;
        shift++;
    }
    return shift;
}

static VAStatus psb_putsurface_buf(
    VADriverContextP ctx,
    VASurfaceID surface,
    unsigned char* data,
    int* data_len,
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
    unsigned short width, height;
    int depth;
    int x,y;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    void *surface_data = NULL;
    int ret;
    
    if (srcw <= destw)
        width = srcw;
    else
        width = destw;

    if (srch <= desth)
        height = srch;
    else
        height = desth;

    object_surface_p obj_surface = SURFACE(surface);
    if (NULL == obj_surface)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    psb_surface_p psb_surface = obj_surface->psb_surface;

    psb__information_message("PutSurface: src    w x h = %d x %d\n", srcw, srch);
    psb__information_message("PutSurface: dest           w x h = %d x %d\n", destw, desth);
    psb__information_message("PutSurface: clipped w x h = %d x %d\n", width, height);
        
    ret = psb_buffer_map(&psb_surface->buf, &surface_data);
    if (ret)
    {
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        goto out;
    }
    
    if (NULL == data)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto out;
    }

#define YUV_420_PLANAR
//#define YUV_420_SEMI_PLANAR

    uint8_t *src_y = surface_data + psb_surface->stride * srcy;
    uint8_t *src_uv = surface_data + psb_surface->stride * (obj_surface->height + srcy / 2);
    uint32_t bytes_per_line = width;

#ifdef YUV_420_SEMI_PLANAR
    for(y = srcy; y < (srcy+height); y += 2)
    {
        uint8_t *dest_y_even = (uint8_t *) (data + y * bytes_per_line);
        uint8_t *dest_y_odd = (uint8_t *) (data + (y + 1) * bytes_per_line);
        uint8_t *dest_uv = (uint8_t *) (data + (height + y/2) * bytes_per_line);
        memcpy(dest_y_even, src_y + srcx, width);
        memcpy(dest_y_odd, (src_y + psb_surface->stride + srcx), width);
        memcpy(dest_uv, src_uv + srcx, width);
        src_y += psb_surface->stride * 2;
        src_uv += psb_surface->stride;
    }
    *data_len = (width * height * 3) >> 1;
#endif
#ifdef YUV_420_PLANAR
    for(y = srcy; y < (srcy+height); y += 4)
    {
        uint8_t *dest_y_0 = (uint8_t *) (data + y * bytes_per_line);
        uint8_t *dest_y_1 = (uint8_t *) (data + (y + 1) * bytes_per_line);
        uint8_t *dest_y_2 = (uint8_t *) (data + (y + 2) * bytes_per_line);
        uint8_t *dest_y_3 = (uint8_t *) (data + (y + 3) * bytes_per_line);
        uint8_t *dest_u = (uint8_t *) (data + (height + y/4) * bytes_per_line);
        uint8_t *dest_v = (uint8_t *) (data + (height + y/4 + (height/4)) * bytes_per_line);
        memcpy(dest_y_0, src_y + srcx, width);
        memcpy(dest_y_1, (src_y + psb_surface->stride + srcx), width);
        memcpy(dest_y_2, (src_y + 2*psb_surface->stride + srcx), width);
        memcpy(dest_y_3, (src_y + 3*psb_surface->stride + srcx), width);
        for (x = srcx; x < (srcx+width); x+= 2) {
            *dest_u = *(src_uv + x);
            *dest_v = *(src_uv + x + 1);
            dest_u++;
            dest_v++;
        }
        src_uv += psb_surface->stride;
        for (x = srcx; x < (srcx+width); x+= 2) {
            *dest_u = *(src_uv + x);
            *dest_v = *(src_uv + x + 1);
            dest_u++;
            dest_v++;
        }
        src_uv += psb_surface->stride;
        src_y += psb_surface->stride * 4;
    }
    *data_len = (width * height * 3) >> 1;
#endif
    
  out:
    if (NULL != surface_data)
    {
        psb_buffer_unmap(&psb_surface->buf);
    }
    
    return vaStatus;
}

static VAStatus psb_putsurface_texture(
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
	psb_output_p output = GET_OUTPUT_DATA(ctx);
        object_surface_p obj_surface;
	int offset = 0;
        psb_surface_p psb_surface;

        obj_surface = SURFACE(surface);
        psb_surface = obj_surface->psb_surface;

        psb_surface->buf.drm_buf;
        psb_surface->buf.pl_flags;

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
        blit_texture_to_buf(ctx, data, srcx, srcy, srcw, srch, destx, desty, destw, desth,
                obj_surface->width, obj_surface->height,
                psb_surface->stride, psb_surface->buf.drm_buf,
                psb_surface->buf.pl_flags);

        psb_android_postBuffer(offset);
}

VAStatus psb_PutSurface(
        VADriverContextP ctx,
        VASurfaceID surface,
        void* android_isurface,
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
    psb_output_p output = GET_OUTPUT_DATA(ctx);
    object_surface_p obj_surface;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    obj_surface = SURFACE(surface);
    if (NULL == obj_surface)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    if (output->dummy_putsurface) {
        psb__information_message("vaPutSurface: dummy mode, return directly\n");
        return VA_STATUS_SUCCESS;
    }

#ifdef ANDROID_VIDEO_TEXTURE_STREAM
    if (!output->register_flag)
    {
        psb_android_register_isurface(android_isurface);
        output->register_flag = 1;
    }

    BC_Video_ioctl_package ioctl_package;
    psb_surface_p psb_surface;
    psb_surface = obj_surface->psb_surface;
    ioctl_package.ioctl_cmd = BC_Video_ioctl_get_buffer_index;
    ioctl_package.inputparam = (int) (wsbmKBufHandle(wsbmKBuf(psb_surface->buf.drm_buf)));

    if(drmCommandWriteRead(driver_data->drm_fd, DRM_BUFFER_CLASS_VIDEO, &ioctl_package, sizeof(ioctl_package)) != 0)
    {
        LOGE("Failed to get buffer index from buffer class video driver (errno=%d).\n", errno);
        return VA_STATUS_ERROR_UNKNOWN;
    }
    psb_android_texture_streaming_display(ioctl_package.outputparam);
    return VA_STATUS_SUCCESS;
#else
    //register Buffer only once
    if (output->output_method == PSB_PUTSURFACE_TEXTURE_XV || 
	output->output_method == PSB_PUTSURFACE_FORCE_TEXTURE_XV) {
	if (!output->register_flag)
	{
	    output->heap_addr = psb_android_registerBuffers(android_isurface, getpid(), srcw, srch);
	    if (VA_STATUS_SUCCESS != vaStatus)
	    {
		psb__information_message("vaPutSurface: register buffers failed, return directly\n");
		return vaStatus;
	    }
	    output->register_flag = 1;
	}

	return psb_putsurface_texture(ctx, surface, output->heap_addr, srcx, srcy, srcw, srch,
                                          destx, desty, destw, desth, flags);
    }

    else if (output->output_method == PSB_PUTSURFACE_OVERLAY_XV ||
             output->output_method == PSB_PUTSURFACE_FORCE_OVERLAY_XV) {
       return psb_putsurface_overlay(ctx, surface, 0,
                                     srcx, srcy, srcw, srch,
                                     destx, desty, destw, desth,
                                     cliprects, number_cliprects, flags
                                     );
    }
#endif    
}

VAStatus psb_PutSurfaceBuf(
    VADriverContextP ctx,
    VASurfaceID surface,
    unsigned char* data,
    int* data_len,
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
    psb_output_p output = GET_OUTPUT_DATA(ctx);
    object_surface_p obj_surface;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    obj_surface = SURFACE(surface);
    if (NULL == obj_surface)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    psb_putsurface_buf(ctx,surface,data,data_len,srcx,srcy,srcw,srch,
                       destx,desty,destw,desth,flags);
    return VA_STATUS_SUCCESS;
}
