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

#ifndef ANDROID
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#endif
#include <va/va_backend.h>
#include "psb_output.h"
#include "psb_surface.h"
#include "psb_buffer.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "psb_xvva.h"
#include "psb_overlay.h"
#include <wsbm/wsbm_manager.h>

#define LOG_TAG "psb_output"
#include <cutils/log.h>

#define INIT_DRIVER_DATA	psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;

#define SURFACE(id)	((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &driver_data->buffer_heap, id ))
#define IMAGE(id)  ((object_image_p) object_heap_lookup( &driver_data->image_heap, id ))
#define SUBPIC(id)  ((object_subpic_p) object_heap_lookup( &driver_data->subpic_heap, id ))
#define CONTEXT(id) ((object_context_p) object_heap_lookup( &driver_data->context_heap, id ))


#define psb__ImageNV12                          \
{                                               \
    VA_FOURCC_NV12,                             \
    VA_LSB_FIRST,                               \
    16,                                         \
    0,                                          \
    0,                                          \
    0,                                          \
    0,                                          \
    0                                           \
}

#define psb__ImageAYUV                          \
{                                               \
    VA_FOURCC_AYUV,                             \
    VA_LSB_FIRST,                               \
    32,                                         \
    0,                                          \
    0,                                          \
    0,                                          \
    0,                                          \
    0                                           \
}

#define psb__ImageAI44                          \
{                                               \
    VA_FOURCC_AI44,                             \
    VA_LSB_FIRST,                               \
    16,                                         \
    0,                                          \
    0,                                          \
    0,                                          \
    0,                                          \
    0,                                          \
}

#define psb__ImageRGBA                          \
{                                               \
    VA_FOURCC_RGBA,                             \
    VA_LSB_FIRST,                               \
    32,                                         \
    32,                                         \
    0xff,                                       \
    0xff00,                                     \
    0xff0000,                                   \
    0xff000000                                  \
}

static VAImageFormat psb__SubpicFormat[] = {
    psb__ImageRGBA,
    psb__ImageAYUV,
    psb__ImageAI44
};

static VAImageFormat psb__CreateImageFormat[] = {
    psb__ImageNV12,
    psb__ImageRGBA,
    psb__ImageAYUV,
    psb__ImageAI44
};
    
VAStatus psb_initOutput(
    VADriverContextP ctx
    )
{
    INIT_DRIVER_DATA;
    psb_output_p video_output;
    int ret = -1;

    video_output = (psb_output_p) malloc(sizeof(psb_output_s));
    if ( NULL == video_output) 
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    memset(video_output,0,sizeof(psb_output_s));
    driver_data->video_output = video_output;
    ret = pthread_mutex_init(&video_output->output_mutex, NULL);
    ASSERT(ret==0);

    if (getenv("PSB_VIDEO_PUTSURFACE_DUMMY")) {
        psb__information_message("vaPutSurface: dummy mode, return directly\n");
        video_output->dummy_putsurface = 0;
        
        return VA_STATUS_SUCCESS;
    }

#if USE_OVERLAY
    video_output->output_method = PSB_PUTSURFACE_OVERLAY_XV;
    if (psbInitVideo(ctx)) {
        psb__information_message("Setup Overlay failed\n");
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    } else {
        psb__information_message("Setup Overlay complete\n");
    }
#else
    video_output->output_method = PSB_PUTSURFACE_X11;
    if (getenv("PSB_VIDEO_PUTSURFACE_X11") == NULL) {
        psb__information_message("Detecting Xvideo port...\n");
        ret = psb_init_xvideo(ctx);	
    }

    if (video_output->output_method == PSB_PUTSURFACE_X11) 
        psb__information_message("vaPutSurface: fall back to SW putsurface\n");
#endif
    
    return VA_STATUS_SUCCESS;
}

VAStatus psb_deinitOutput(
    VADriverContextP ctx
    )
{
    INIT_DRIVER_DATA;
    psb_output_p video_output = GET_OUTPUT_DATA(ctx);

    if (NULL == video_output)
        return VA_STATUS_SUCCESS;

#if USE_OVERLAY
    if (psbDeInitVideo(ctx)) {
        psb__information_message("Deinitialize Overlay failed\n");
    }
#else
    if (video_output->output_method != PSB_PUTSURFACE_X11)
	psb_deinit_xvideo(ctx);
#endif

    pthread_mutex_destroy(&video_output->output_mutex);
    free(video_output);
    driver_data->video_output = NULL;

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


static VAStatus psb_putsurface_x11(
    VADriverContextP ctx,
    VASurfaceID surface,
    Drawable draw, /* X Drawable */
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
#ifndef ANDROID
    GC gc;
    XImage *ximg = NULL;
    Visual *visual;
    unsigned short width, height;
    int depth;
    int x = 0,y = 0;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    void *surface_data = NULL;
    int ret;
    
    uint32_t rmask = 0;
    uint32_t gmask = 0;
    uint32_t bmask = 0;
    
    uint32_t rshift = 0;
    uint32_t gshift = 0;
    uint32_t bshift = 0;

    void yuv2pixel(uint32_t *pixel, int y, int u, int v)
    {
        int r, g, b;
        /* Warning, magic values ahead */
        r = y + ((351 * (v-128)) >> 8);
        g = y - (((179 * (v-128)) + (86 * (u-128))) >> 8);
        b = y + ((444 * (u-128)) >> 8);
	
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;
			
        *pixel = ((r << rshift) & rmask) | ((g << gshift) & gmask) | ((b << bshift) & bmask);
    }

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

    psb__information_message("PutSurface: src	  w x h = %d x %d\n", srcw, srch);
    psb__information_message("PutSurface: dest 	  w x h = %d x %d\n", destw, desth);
    psb__information_message("PutSurface: clipped w x h = %d x %d\n", width, height);
        
    visual = DefaultVisual(ctx->x11_dpy, ctx->x11_screen);
    gc = XCreateGC(ctx->x11_dpy, draw, 0, NULL);
    depth = DefaultDepth(ctx->x11_dpy, ctx->x11_screen);

    if (TrueColor != visual->class)
    {
        psb__error_message("PutSurface: Default visual of X display must be TrueColor.\n");
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        goto out;
    }
    
    ret = psb_buffer_map(&psb_surface->buf, &surface_data);
    if (ret)
    {
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        goto out;
    }
    
    rmask = visual->red_mask;
    gmask = visual->green_mask;
    bmask = visual->blue_mask;

    rshift = mask2shift(rmask);
    gshift = mask2shift(gmask);
    bshift = mask2shift(bmask);
    
    psb__information_message("PutSurface: Pixel masks: R = %08x G = %08x B = %08x\n", rmask, gmask, bmask); 
    psb__information_message("PutSurface: Pixel shifts: R = %d G = %d B = %d\n", rshift, gshift, bshift); 
    
    ximg = XCreateImage(ctx->x11_dpy, visual, depth, ZPixmap, 0, NULL, width, height, 32, 0 );

    if (ximg->byte_order == MSBFirst)
        psb__information_message("PutSurface: XImage pixels has MSBFirst, %d bits / pixel\n", ximg->bits_per_pixel);
    else
        psb__information_message("PutSurface: XImage pixels has LSBFirst, %d bits / pixel\n", ximg->bits_per_pixel);

    if (ximg->bits_per_pixel != 32)
    {
        psb__error_message("PutSurface: Display uses %d bits/pixel which is not supported\n");
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        goto out;
    }
    
    ximg->data = (char *) malloc(ximg->bytes_per_line * height);
    if (NULL == ximg->data)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto out;
    }

    uint8_t *src_y = surface_data + psb_surface->stride * srcy;
    uint8_t *src_uv = surface_data + psb_surface->stride * (obj_surface->height + srcy / 2);

    for(y = srcy; y < (srcy+height); y += 2)
    {
        uint32_t *dest_even = (uint32_t *) (ximg->data + y * ximg->bytes_per_line);
        uint32_t *dest_odd = (uint32_t *) (ximg->data + (y + 1) * ximg->bytes_per_line);
        for(x = srcx; x < (srcx+width); x += 2)
        {
            /* Y1 Y2 */
            /* Y3 Y4 */
            int y1 = *(src_y + x);
            int y2 = *(src_y + x + 1);
            int y3 = *(src_y + x + psb_surface->stride);
            int y4 = *(src_y + x + psb_surface->stride + 1);

            /* U V */
            int u = *(src_uv + x);
            int v = *(src_uv + x +1 );
			
            yuv2pixel(dest_even++, y1, u, v);
            yuv2pixel(dest_even++, y2, u, v);

            yuv2pixel(dest_odd++, y3, u, v);
            yuv2pixel(dest_odd++, y4, u, v);
        }
        src_y += psb_surface->stride * 2;
        src_uv += psb_surface->stride;
    }
    
    XPutImage(ctx->x11_dpy, draw, gc, ximg, 0, 0, destx, desty, width, height);
    XFlush(ctx->x11_dpy);
                  
  out:
    if (NULL != ximg)
    {
        XDestroyImage(ximg);
    }
    if (NULL != surface_data)
    {
        psb_buffer_unmap(&psb_surface->buf);
    }
    XFreeGC(ctx->x11_dpy, gc);
    
    return vaStatus;
#else
    return VA_STATUS_SUCCESS;
#endif
}

#ifdef ANDROID
static VAStatus psb_putsurface_buf(
    VADriverContextP ctx,
    VASurfaceID surface,
    Drawable draw, /* X Drawable */
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
#endif

VAStatus psb_PutSurface(
    VADriverContextP ctx,
    VASurfaceID surface,
    Drawable draw, /* X Drawable */
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

#ifndef ANDROID
    if (output->output_method == PSB_PUTSURFACE_X11) {
        psb_putsurface_x11(ctx,surface,draw,srcx,srcy,srcw,srch,
                           destx,desty,destw,desth,flags);
        return VA_STATUS_SUCCESS;
    }

    return psb_putsurface_xvideo(ctx, surface, draw,
                                 srcx, srcy, srcw, srch,
                                 destx, desty, destw, desth,
                                 cliprects, number_cliprects, flags
                                 );
#else
    return psb_putsurface_overlay(ctx, surface, draw,
                                 srcx, srcy, srcw, srch,
                                 destx, desty, destw, desth,
                                 cliprects, number_cliprects, flags
                                 );
#endif
}

#ifdef ANDROID
VAStatus psb_PutSurfaceBuf(
    VADriverContextP ctx,
    VASurfaceID surface,
    Drawable draw, /* X Drawable */
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

    psb_putsurface_buf(ctx,surface,draw,data,data_len,srcx,srcy,srcw,srch,
                       destx,desty,destw,desth,flags);
    return VA_STATUS_SUCCESS;
}
#endif

#ifndef VA_STATUS_ERROR_INVALID_IMAGE_FORMAT
#define VA_STATUS_ERROR_INVALID_IMAGE_FORMAT VA_STATUS_ERROR_UNKNOWN
#endif

static VAImageFormat *psb__VAImageCheckFourCC(
    VAImageFormat 	*src_format,
    VAImageFormat 	*dst_format,
    int          	dst_num
)
{
    int i;
    if (NULL == src_format || dst_format == NULL)
    {
        return NULL;
    }

    /* check VAImage at first */
    for (i=0; i<dst_num; i++) {
        if (dst_format[i].fourcc == src_format->fourcc)
            return &dst_format[i];
    }
    
    psb__error_message("Unsupport fourcc 0x%x\n",src_format->fourcc);
    return NULL;
}

static void psb__VAImageCheckRegion(
    object_surface_p surface,
    VAImage *image,
    int *src_x,
    int *src_y,
    int *dest_x,
    int *dest_y, 
    unsigned int *width,
    unsigned int *height
)
{
    /* check for image */
    if (*src_x < 0) *src_x = 0;
    if (*src_x > image->width) *src_x = image->width - 1;
    if (*src_y < 0) *src_y = 0;
    if (*src_y > image->height) *src_y = image->height - 1;
    
    if (((*width) + (*src_x)) > image->width) *width = image->width - *src_x;
    if (((*height) + (*src_y)) > image->height) *height = image->height - *src_x;

    /* check for surface */
    if (*dest_x < 0) *dest_x = 0;
    if (*dest_x > surface->width) *dest_x = surface->width - 1;
    if (*dest_y < 0) *dest_y = 0;
    if (*dest_y > surface->height) *dest_y = surface->height - 1;

    if (((*width) + (*dest_x)) > surface->width) *width = surface->width - *dest_x;
    if (((*height) + (*dest_y)) > surface->height) *height = surface->height - *dest_x;
}


VAStatus psb_QueryImageFormats(
	VADriverContextP ctx,
	VAImageFormat *format_list,        /* out */
	int *num_formats           /* out */
)
{
   VAStatus vaStatus = VA_STATUS_SUCCESS;
   
   if(NULL == format_list)
   {
	vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
	DEBUG_FAILURE;
	return vaStatus;
   }
   if(NULL == num_formats)
   {
	vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
	DEBUG_FAILURE;
	return vaStatus;
   }
    memcpy(format_list,psb__CreateImageFormat,sizeof(psb__CreateImageFormat));
    *num_formats = PSB_MAX_IMAGE_FORMATS;
    
    return VA_STATUS_SUCCESS;
}

inline int min_POT(int n)
{
    if ((n & (n-1)) == 0) /* already POT */
        return n;

    return n |= n>>16, n |= n>>8, n |= n>>4, n |= n>>2, n |= n>>1, n + 1;
    /* return ((((n |= n>>16) |= n>>8) |= n>>4) |= n>>2) |= n>>1, n + 1; */
}

VAStatus psb_CreateImage(
	VADriverContextP ctx,
	VAImageFormat *format,
	int width,
	int height,
	VAImage *image     /* out */
)
{
    INIT_DRIVER_DATA;
    VAImageID imageID;
    object_image_p obj_image;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    VAImageFormat *img_fmt;
    int pitch_pot;
    
    (void)driver_data;

    img_fmt = psb__VAImageCheckFourCC(format, psb__CreateImageFormat,
                                      sizeof(psb__CreateImageFormat)/sizeof(VAImageFormat));
    if (img_fmt == NULL)
        return VA_STATUS_ERROR_UNKNOWN;

    imageID = object_heap_allocate( &driver_data->image_heap );
    obj_image = IMAGE(imageID);
    if (NULL == obj_image)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        return vaStatus;
    }

    obj_image->image.image_id = imageID;
    obj_image->image.format = *img_fmt;
    obj_image->subpic_ref = 0;

    pitch_pot = min_POT(width);
    
    switch (format->fourcc) {
        case VA_FOURCC_NV12:
        {
            obj_image->image.width = width;
            obj_image->image.height = height;
            obj_image->image.data_size = pitch_pot*height /*Y*/ + 2* (pitch_pot/2)*(height/2);/*UV*/
            obj_image->image.num_planes = 2;
            obj_image->image.pitches[0] = pitch_pot;
            obj_image->image.pitches[1] = pitch_pot;
            obj_image->image.offsets[0] = 0;
            obj_image->image.offsets[1] = pitch_pot*height;
            obj_image->image.num_palette_entries = 0;
            obj_image->image.entry_bytes = 0;
            obj_image->image.component_order[0] = 'Y';
            obj_image->image.component_order[1] = 'U';/* fixed me: packed UV packed here! */
            obj_image->image.component_order[2] = 'V';
            obj_image->image.component_order[3] = '\0';
            break;
        }
        case VA_FOURCC_AYUV:
        {
            obj_image->image.width = width;
            obj_image->image.height = height;
            obj_image->image.data_size = 4*pitch_pot*height;
            obj_image->image.num_planes = 1;
            obj_image->image.pitches[0] = 4*pitch_pot;
            obj_image->image.num_palette_entries = 0;
            obj_image->image.entry_bytes = 0;
            obj_image->image.component_order[0] = 'V';
            obj_image->image.component_order[1] = 'U';
            obj_image->image.component_order[2] = 'Y';
            obj_image->image.component_order[3] = 'A';
            break;
        }
        case VA_FOURCC_RGBA:
        {
            obj_image->image.width = width;
            obj_image->image.height = height;
            obj_image->image.data_size = 4*pitch_pot*height;
            obj_image->image.num_planes = 1;
            obj_image->image.pitches[0] = 4*pitch_pot;
            obj_image->image.num_palette_entries = 0;
            obj_image->image.entry_bytes = 0;
            obj_image->image.component_order[0] = 'R';
            obj_image->image.component_order[1] = 'G';
            obj_image->image.component_order[2] = 'B';
            obj_image->image.component_order[3] = 'A';
            break;
        }
        case VA_FOURCC_AI44:
        {
            obj_image->image.width = width;
            obj_image->image.height = height;
            obj_image->image.data_size = pitch_pot*height;/* one byte one element */
            obj_image->image.num_planes = 1;
            obj_image->image.pitches[0] = pitch_pot;
            obj_image->image.num_palette_entries = 16;
            obj_image->image.entry_bytes = 4; /* AYUV */
            obj_image->image.component_order[0] = 'I';
            obj_image->image.component_order[1] = 'A';
            obj_image->image.component_order[2] = '\0';
            obj_image->image.component_order[3] = '\0';
            break;
        }
        case VA_FOURCC_IYUV:
        {
            obj_image->image.width = width;
            obj_image->image.height = height;
            obj_image->image.data_size = pitch_pot*height /*Y*/ + 2* (pitch_pot/2)*(height/2);/*UV*/
            obj_image->image.num_planes = 3;
            obj_image->image.pitches[0] = pitch_pot;
            obj_image->image.pitches[1] = pitch_pot/2;
            obj_image->image.pitches[2] = pitch_pot/2;
            obj_image->image.offsets[0] = 0;
            obj_image->image.offsets[1] = pitch_pot*height;
            obj_image->image.offsets[2] = pitch_pot*height + (pitch_pot/2) * (height/2);
            obj_image->image.num_palette_entries = 0;
            obj_image->image.entry_bytes = 0;
            obj_image->image.component_order[0] = 'Y';
            obj_image->image.component_order[1] = 'U';
            obj_image->image.component_order[2] = 'V';
            obj_image->image.component_order[3] = '\0';
            break;
        }
        default:
        {
            vaStatus = VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
            break;
        }
    }
    
    if (VA_STATUS_SUCCESS == vaStatus) {
        /* create the buffer */
        vaStatus = psb__CreateBuffer(driver_data, NULL, VAImageBufferType,
                                     obj_image->image.data_size, 1, NULL, &obj_image->image.buf);
    }

    obj_image->derived_surface = 0;
    
    if (VA_STATUS_SUCCESS != vaStatus)
    {
        object_heap_free( &driver_data->image_heap, (object_base_p) obj_image);
    }
    else
    {
        memcpy(image, &obj_image->image, sizeof(VAImage));
    }
    
    return vaStatus;
}

VAStatus psb_DeriveImage(
	VADriverContextP ctx,
	VASurfaceID surface,
	VAImage *image     /* out */
)
{
    INIT_DRIVER_DATA;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    VABufferID bufferID;
    object_buffer_p obj_buffer;
    VAImageID imageID;
    object_image_p obj_image;
    object_surface_p obj_surface = SURFACE(surface);
    unsigned int fourcc,fourcc_index=~0,i;
    uint32_t srf_buf_ofs = 0;
    
    if (NULL == obj_surface) {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    fourcc = obj_surface->psb_surface->extra_info[4];
    for (i=0;i<PSB_MAX_IMAGE_FORMATS;i++) {
        if (psb__CreateImageFormat[i].fourcc == fourcc) {
            fourcc_index = i;
            break;
        }
    }
    if (i==PSB_MAX_IMAGE_FORMATS) {
        psb__error_message("Can't support the Fourcc\n");
        vaStatus = VA_STATUS_ERROR_OPERATION_FAILED;
        return vaStatus;
    }

    /* create the image */
    imageID = object_heap_allocate( &driver_data->image_heap );
    obj_image = IMAGE(imageID);
    if (NULL == obj_image)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        return vaStatus;
    }

    /* create a buffer to represent surface buffer */
    bufferID = object_heap_allocate( &driver_data->buffer_heap );
    obj_buffer = BUFFER(bufferID);
    if (NULL == obj_buffer) {
        object_heap_free( &driver_data->image_heap, (object_base_p) obj_image);
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE;
        return vaStatus;
    }
    obj_buffer->type = VAImageBufferType;
    obj_buffer->buffer_data = NULL;
    obj_buffer->psb_buffer = &obj_surface->psb_surface->buf;
    obj_buffer->size = obj_surface->psb_surface->size;
    obj_buffer->max_num_elements = 0;
    obj_buffer->alloc_size = obj_buffer->size;

    /* fill obj_image data structure */
    obj_image->image.image_id = imageID;
    obj_image->image.format = psb__CreateImageFormat[fourcc_index];
    obj_image->subpic_ref = 0;

    obj_image->image.buf = bufferID;
    obj_image->image.width = obj_surface->width;
    obj_image->image.height = obj_surface->height;
    obj_image->image.data_size = obj_surface->psb_surface->size;

    srf_buf_ofs = obj_surface->psb_surface->buf.buffer_ofs;
    
    switch (fourcc) {
        case VA_FOURCC_NV12:
        {
            obj_image->image.num_planes = 2;
            obj_image->image.pitches[0] = obj_surface->psb_surface->stride;
            obj_image->image.pitches[1] = obj_surface->psb_surface->stride;
            
            obj_image->image.offsets[0] = srf_buf_ofs;
            obj_image->image.offsets[1] = srf_buf_ofs + obj_surface->height * obj_surface->psb_surface->stride;
            obj_image->image.num_palette_entries = 0;
            obj_image->image.entry_bytes = 0;
            obj_image->image.component_order[0] = 'Y';
            obj_image->image.component_order[1] = 'U';/* fixed me: packed UV packed here! */
            obj_image->image.component_order[2] = 'V';
            obj_image->image.component_order[3] = '\0';
            break;
        }
        default:
            break;
    }
    
    obj_image->derived_surface = surface; /* this image is derived from a surface */
    obj_surface->derived_imgcnt++;
    
    memcpy(image, &obj_image->image, sizeof(VAImage));
    
    return vaStatus;
}

VAStatus psb__destroy_image(psb_driver_data_p driver_data, object_image_p obj_image)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    
    if (obj_image->subpic_ref > 0) {
        vaStatus = VA_STATUS_ERROR_OPERATION_FAILED;
        return vaStatus;
    }

    object_surface_p obj_surface = SURFACE(obj_image->derived_surface);
    
    if (obj_surface == NULL) { /* destroy the buffer */
        object_buffer_p obj_buffer = BUFFER(obj_image->image.buf);
        if (NULL == obj_buffer)
        {
            vaStatus = VA_STATUS_ERROR_INVALID_BUFFER;
            DEBUG_FAILURE;
            return vaStatus;
        }
        psb__suspend_buffer(driver_data, obj_buffer);
    } else {
        object_buffer_p obj_buffer = BUFFER(obj_image->image.buf);
        object_heap_free(&driver_data->buffer_heap,&obj_buffer->base);
        obj_surface->derived_imgcnt--;
    }
    object_heap_free( &driver_data->image_heap, (object_base_p) obj_image);

    return VA_STATUS_SUCCESS;
}

VAStatus psb_DestroyImage(
	VADriverContextP ctx,
	VAImageID image
)
{
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_image_p obj_image;
    
    obj_image = IMAGE(image);
    if (NULL == obj_image)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_IMAGE;
        DEBUG_FAILURE;
        return vaStatus;
    }
    return psb__destroy_image(driver_data, obj_image);
}

VAStatus psb_SetImagePalette (
	VADriverContextP ctx,
	VAImageID image,
	/* 
	 * pointer to an array holding the palette data.  The size of the array is
	 * num_palette_entries * entry_bytes in size.  The order of the components 
	 * in the palette is described by the component_order in VAImage struct
	 */
	unsigned char *palette 
)
{
    INIT_DRIVER_DATA;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    
    object_image_p obj_image = IMAGE(image);
    if (NULL == obj_image)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_IMAGE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    if (obj_image->image.format.fourcc != VA_FOURCC_AI44) {
        /* only support AI44 palette */
        vaStatus = VA_STATUS_ERROR_OPERATION_FAILED;
        return vaStatus;
    }

    memcpy(obj_image->palette, palette, obj_image->image.num_palette_entries * sizeof(unsigned int));
    
    return vaStatus;
}

static VAStatus lnc_unpack_topaz_rec(int src_width,int src_height,
                                 unsigned char *p_srcY,unsigned char *p_srcUV,
                                 unsigned char *p_dstY,unsigned char *p_dstU,unsigned char *p_dstV,
                                 int dstY_stride,int dstU_stride,int dstV_stride,
                                 int surface_height)
{
    unsigned char *tmp_dstY=NULL;
    unsigned char *tmp_dstUV=NULL;

    int n,i,index;

    psb__information_message("Unpack reconstructed frame to image\n");
    
    /* do this one column at a time. */
    tmp_dstY = (unsigned char *)malloc(16*src_height);
    if (tmp_dstY==NULL) 
        return  VA_STATUS_ERROR_ALLOCATION_FAILED;
    
    tmp_dstUV = (unsigned char*)malloc(16*src_height/2);
    if (tmp_dstUV == NULL) {
        free(tmp_dstY);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
        
    /*  Copy Y data */
    for (n=0;n<src_width/16;n++)
    {
        memcpy((void*)tmp_dstY,p_srcY,16*src_height);
        p_srcY+=(16 * surface_height);
        for(i=0;i<src_height;i++)
        {
            memcpy(p_dstY + dstY_stride*i + n*16,tmp_dstY + 16 * i,16);
        }
    }

    /* Copy U/V data */
    for(n=0;n<src_width/16;n++)
    {
        memcpy((void*)tmp_dstUV,p_srcUV,16*src_height/2);
        p_srcUV+=(16*surface_height/2);
        for(i=0;i<src_height/2;i++)
        {
            for(index=0;index<8;index++)
            {
                p_dstU[i*dstU_stride + n*8 + index]=tmp_dstUV[index*2 + i*16];
                p_dstV[i*dstV_stride + n*8 + index]=tmp_dstUV[index*2 + i*16+1];
            }
        }
    }
    if (tmp_dstY)
        free(tmp_dstY);
    if (tmp_dstUV)
        free(tmp_dstUV);

    return VA_STATUS_SUCCESS;
}


VAStatus psb_GetImage(
    VADriverContextP ctx,
    VASurfaceID surface,
    int x,     /* coordinates of the upper left source pixel */
    int y,
    unsigned int width, /* width and height of the region */
    unsigned int height,
    VAImageID image_id
)
{
    INIT_DRIVER_DATA;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int ret,src_x=0,src_y=0,dest_x=0, dest_y=0;
    
    (void)driver_data;
    (void)lnc_unpack_topaz_rec;
    
    object_image_p obj_image = IMAGE(image_id);
    if (NULL == obj_image)
    {
        psb__error_message("Invalidate Image\n");
        
        vaStatus = VA_STATUS_ERROR_INVALID_IMAGE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    if (obj_image->image.format.fourcc != VA_FOURCC_NV12) {
        psb__error_message("target VAImage fourcc should be NV12 or IYUV\n");
        vaStatus = VA_STATUS_ERROR_OPERATION_FAILED;
        return vaStatus;
    }
    
    object_surface_p obj_surface = SURFACE(surface);
    if (NULL == obj_surface)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }
    
    psb__VAImageCheckRegion(obj_surface,&obj_image->image,&src_x,&src_y,&dest_x,&dest_y,&width,&height);

    psb_surface_p psb_surface = obj_surface->psb_surface;
    void *surface_data;
    ret = psb_buffer_map(&psb_surface->buf, &surface_data);
    if (ret) {
        return VA_STATUS_ERROR_UNKNOWN;
    }

    object_buffer_p obj_buffer = BUFFER(obj_image->image.buf);
    if (NULL == obj_buffer)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_BUFFER;
        DEBUG_FAILURE;
        return vaStatus;
    }

    void *image_data;
    ret = psb_buffer_map(obj_buffer->psb_buffer, &image_data);
    if (ret) {
        psb__error_message("Map buffer failed\n");
                
        psb_buffer_unmap(&psb_surface->buf);
        return VA_STATUS_ERROR_UNKNOWN;
    }

    image_data += obj_surface->psb_surface->buf.buffer_ofs;
    
    switch (obj_image->image.format.fourcc) {
        case VA_FOURCC_NV12:
        {
            unsigned char *source_y,*src_uv,*dst_y,*dst_uv;
            int i;
            /* copy Y plane */
            dst_y = image_data;
            source_y = surface_data + y*psb_surface->stride + x;
            for (i=0; i<height;i++)  {
                memcpy(dst_y, source_y,width);
                dst_y += obj_image->image.pitches[0];
                source_y += psb_surface->stride;
            }
        
            /* copy UV plane */
            dst_uv = image_data + obj_image->image.offsets[1];
            src_uv = surface_data + psb_surface->stride * obj_surface->height + (y/2)*psb_surface->stride + x;;
            for (i=0; i<obj_image->image.height/2; i++) {
                memcpy(dst_uv, src_uv, width);
                dst_uv += obj_image->image.pitches[1];
                src_uv += psb_surface->stride;
            }
	    break;
        }
#if 0        
        case VA_FOURCC_IYUV:
        {
            unsigned char *source_y,*dst_y;
            unsigned char *source_uv, *source_u, *source_v, *dst_u, *dst_v;
            unsigned int i;

            if (psb_surface->extra_info[4] == VA_FOURCC_IREC) {
                /* copy Y plane */
                dst_y = image_data + obj_image->image.offsets[0] + src_y * obj_image->image.pitches[0] + src_x;
                dst_u = image_data + obj_image->image.offsets[1] + src_y * obj_image->image.pitches[1] + src_x;
                dst_v = image_data + obj_image->image.offsets[2] + src_y * obj_image->image.pitches[2] + src_x;

                source_y = surface_data + dest_y*psb_surface->stride + dest_x;
                source_uv = surface_data + obj_surface->height * psb_surface->stride
                    + dest_y*(psb_surface->stride/2) + dest_x;

                vaStatus = lnc_unpack_topaz_rec(width, height, source_y, source_uv,
		                   dst_y, dst_u, dst_v,
		                   obj_image->image.pitches[0],
		                   obj_image->image.pitches[1],
		                   obj_image->image.pitches[2],                                     
		                   obj_surface->height);
            }
            
            break;
        }
#endif        
        default:
            break;
    }
    psb_buffer_unmap(obj_buffer->psb_buffer);
    psb_buffer_unmap(&psb_surface->buf);
    
    return vaStatus;
}

static VAStatus psb_PutImage2(
    VADriverContextP ctx,
    VASurfaceID surface,
    VAImageID image_id,
    int src_x,
    int src_y,
    unsigned int width,
    unsigned int height,
    int dest_x,
    int dest_y 
)
{
    INIT_DRIVER_DATA;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int ret;

    object_image_p obj_image = IMAGE(image_id);
    if (NULL == obj_image)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_IMAGE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    object_surface_p obj_surface = SURFACE(surface);
    if (NULL == obj_surface)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    if (obj_image->image.format.fourcc != VA_FOURCC_NV12) {
        psb__error_message("target VAImage fourcc should be NV12 or IYUV\n");
        vaStatus = VA_STATUS_ERROR_OPERATION_FAILED;
        return vaStatus;
    }

    psb__VAImageCheckRegion(obj_surface,&obj_image->image,&src_x,&src_y,&dest_x,&dest_y,&width,&height);

    psb_surface_p psb_surface = obj_surface->psb_surface;
    void *surface_data;
    ret = psb_buffer_map(&psb_surface->buf, &surface_data);
    if (ret) {
        return VA_STATUS_ERROR_UNKNOWN;
    }

    object_buffer_p obj_buffer = BUFFER(obj_image->image.buf);
    if (NULL == obj_buffer)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_BUFFER;
        DEBUG_FAILURE;
        return vaStatus;
    }

    void *image_data;
    ret = psb_buffer_map(obj_buffer->psb_buffer, &image_data);
    if (ret) {
        psb_buffer_unmap(&psb_surface->buf);
        return VA_STATUS_ERROR_UNKNOWN;
    }

    image_data += obj_surface->psb_surface->buf.buffer_ofs;
    
    switch (obj_image->image.format.fourcc) {
        case VA_FOURCC_NV12:
        {
            char *source_y,*src_uv,*dst_y,*dst_uv;
            unsigned int i;
        
            /* copy Y plane */
            source_y = image_data + obj_image->image.offsets[0] + src_y * obj_image->image.pitches[0] + src_x;
            dst_y = surface_data + dest_y*psb_surface->stride + dest_x;
            for (i=0; i<height;i++)  {
                memcpy(dst_y, source_y,width);
                source_y += obj_image->image.pitches[0];
                dst_y += psb_surface->stride;
            }
        
            /* copy UV plane */
            src_uv = image_data + obj_image->image.offsets[1] + (src_y/2) * obj_image->image.pitches[1] + src_x;
            dst_uv = surface_data + psb_surface->stride * obj_surface->height + (dest_y/2)*psb_surface->stride + dest_x;
            for (i=0; i<obj_image->image.height/2; i++) {
                memcpy(dst_uv, src_uv, width);
                src_uv += obj_image->image.pitches[1];
                dst_uv += psb_surface->stride;
            }
            break;
        }
#if 0        
        case VA_FOURCC_IYUV:
        {
            char *source_y,*dst_y;
            char *source_u, *source_v, *dst_u, *dst_v;
            unsigned int i;
        
            /* copy Y plane */
            source_y = image_data + obj_image->image.offsets[0] + src_y * obj_image->image.pitches[0] + src_x;
            source_u = image_data + obj_image->image.offsets[1] + src_y * obj_image->image.pitches[1] + src_x;
            source_v = image_data + obj_image->image.offsets[2] + src_y * obj_image->image.pitches[2] + src_x;

            dst_y = surface_data + dest_y*psb_surface->stride + dest_x;
            dst_u = surface_data + obj_surface->height * psb_surface->stride
                + dest_y*(psb_surface->stride/2) + dest_x;
            dst_v = surface_data + obj_surface->height * psb_surface->stride
                + (obj_surface->height/2) * (psb_surface->stride/2)
                + dest_y*(psb_surface->stride/2) + dest_x;
            
            for (i=0; i<height;i++)  {
                memcpy(dst_y, source_y,width);
                source_y += obj_image->image.pitches[0];
                dst_y += psb_surface->stride;
            }
        
            /* copy UV plane */
            for (i=0; i<obj_image->image.height/2; i++) {
                memcpy(dst_u, source_u, width);
                memcpy(dst_v, source_v, width);
                
                source_u += obj_image->image.pitches[1];
                source_v += obj_image->image.pitches[2];
                
                dst_u += psb_surface->stride/2;
                dst_v += psb_surface->stride/2;
            }
            break;
        }
#endif        
        default:
            break;
    }
    
    psb_buffer_unmap(obj_buffer->psb_buffer);
    psb_buffer_unmap(&psb_surface->buf);
    
    return VA_STATUS_SUCCESS;
}


static void psb__VAImageCheckRegion2(
    object_surface_p surface,
    VAImage *image,
    int *src_x,
    int *src_y,
    unsigned int *src_width,
    unsigned int *src_height,
    int *dest_x,
    int *dest_y,
    unsigned int *dest_width,
    unsigned int *dest_height
)
{
    /* check for image */
    if (*src_x < 0) *src_x = 0;
    if (*src_x > image->width) *src_x = image->width - 1;
    if (*src_y < 0) *src_y = 0;
    if (*src_y > image->height) *src_y = image->height - 1;
    
    if (((*src_width) + (*src_x)) > image->width) *src_width = image->width - *src_x;
    if (((*src_height) + (*src_y)) > image->height) *src_height = image->height - *src_x;

    /* check for surface */
    if (*dest_x < 0) *dest_x = 0;
    if (*dest_x > surface->width) *dest_x = surface->width - 1;
    if (*dest_y < 0) *dest_y = 0;
    if (*dest_y > surface->height) *dest_y = surface->height - 1;

    if (((*dest_width) + (*dest_x)) > surface->width) *dest_width = surface->width - *dest_x;
    if (((*dest_height) + (*dest_y)) > surface->height) *dest_height = surface->height - *dest_x;
}

VAStatus psb_PutImage(
    VADriverContextP ctx,
    VASurfaceID surface,
    VAImageID image_id,
    int src_x,
    int src_y,
    unsigned int src_width,
    unsigned int src_height,
    int dest_x,
    int dest_y,
    unsigned int dest_width,
    unsigned int dest_height
)
{
    INIT_DRIVER_DATA;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int ret;
    
    if ((src_width == dest_width) && (src_height == dest_height))
    {
        /* Shortcut if scaling is not required */
        return psb_PutImage2(ctx, surface, image_id, src_x, src_y, src_width, src_height, dest_x, dest_y);
    }

    object_image_p obj_image = IMAGE(image_id);
    if (NULL == obj_image)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_IMAGE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    if (obj_image->image.format.fourcc != VA_FOURCC_NV12) {
        /* only support NV12 getImage/putImage */
        vaStatus = VA_STATUS_ERROR_OPERATION_FAILED;
        return vaStatus;
    }

    object_surface_p obj_surface = SURFACE(surface);
    if (NULL == obj_surface)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    psb__VAImageCheckRegion2(obj_surface,&obj_image->image,
                             &src_x,&src_y,&src_width,&src_height,
                             &dest_x,&dest_y,&dest_width,&dest_height);
    
    psb_surface_p psb_surface = obj_surface->psb_surface;
    void *surface_data;
    ret = psb_buffer_map(&psb_surface->buf, &surface_data);
    if (ret) {
        return VA_STATUS_ERROR_UNKNOWN;
    }

    object_buffer_p obj_buffer = BUFFER(obj_image->image.buf);
    if (NULL == obj_buffer)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_BUFFER;
        DEBUG_FAILURE;
        return vaStatus;
    }

    void *image_data;
    ret = psb_buffer_map(obj_buffer->psb_buffer, &image_data);
    if (ret) {
        psb_buffer_unmap(&psb_surface->buf);
        return VA_STATUS_ERROR_UNKNOWN;
    }

    /* just a prototype, the algorithm is ugly and not optimized */
    switch (obj_image->image.format.fourcc) {
        case VA_FOURCC_NV12:
        {
            unsigned char *source_y,*dst_y;
            unsigned short *source_uv,*dst_uv;
            unsigned int i,j;
            float xratio = (float) src_width / dest_width;
            float yratio = (float) src_height / dest_height;
            
            /* dst_y/dst_uv: Y/UV plane of destination */
            dst_y = surface_data + dest_y*psb_surface->stride + dest_x;
            dst_uv = surface_data + psb_surface->stride * obj_surface->height
                    + (dest_y/2)*psb_surface->stride + dest_x;
            
            for (j=0; j<dest_height;j++)  {
                unsigned char *dst_y_tmp = dst_y;
                unsigned short *dst_uv_tmp = dst_uv;
                
                for (i=0; i<dest_width;i++)  {
                    int x = (int)(i * xratio);
                    int y = (int)(j * yratio);

                    source_y = image_data + obj_image->image.offsets[0]
                        + (src_y + y) * obj_image->image.pitches[0]
                        + (src_x + x);
                    *dst_y_tmp = *source_y;
                    dst_y_tmp++;
                    
                    if (((i&1) == 0)) {
                        source_uv = (unsigned short *)(image_data + obj_image->image.offsets[1]
                                                      + ((src_y + y)/2) * obj_image->image.pitches[1])
                            + ((src_x + x)/2);
                        *dst_uv_tmp = *source_uv;
                        dst_uv_tmp++;                        
                    }
                }
                dst_y += psb_surface->stride;
                
                if (j&1)
                    dst_uv = (unsigned short *)((void *)dst_uv + psb_surface->stride);
            }
            break;            
        }
        default:/* will not reach here */
            break;
    }
        
    psb_buffer_unmap(obj_buffer->psb_buffer);
    psb_buffer_unmap(&psb_surface->buf);
    
    return VA_STATUS_SUCCESS;
}

/*
 * Link supbicture into one surface, when update is zero, not need to
 * update the location information
 * The image informatio and its BO of subpicture will copied to surface
 * so need to update it when a vaSetSubpictureImage is called
 */
static VAStatus psb__LinkSubpictIntoSurface(
    psb_driver_data_p driver_data,
    object_surface_p obj_surface,
    object_subpic_p obj_subpic,
    short src_x,
    short src_y,
    unsigned short src_w,
    unsigned short src_h,
    short dest_x,
    short dest_y,
    unsigned short dest_w,
    unsigned short dest_h,
    int update /* update subpicture location */
)
{
    PsbVASurfaceRec *surface_subpic;
    object_image_p obj_image=IMAGE(obj_subpic->image_id);
    if (NULL == obj_image)
    {
        return VA_STATUS_ERROR_INVALID_IMAGE;
    }

    VAImage *image=&obj_image->image;
    object_buffer_p obj_buffer = BUFFER(image->buf);
    if (NULL == obj_buffer)
    {
        return VA_STATUS_ERROR_INVALID_BUFFER;
    }

    int found = 0;
    
    if (obj_surface->subpictures != NULL) {
        surface_subpic = ( PsbVASurfaceRec *)obj_surface->subpictures;
        do {
            if (surface_subpic->subpic_id == obj_subpic->subpic_id) {
                found = 1;
                break;
            } else
                surface_subpic = surface_subpic->next;
        } while (surface_subpic);
    }

    if (found == 0) { /* new node */
        if (obj_surface->subpic_count >= PSB_SUBPIC_MAX_NUM) {
            psb__error_message("can't support so many sub-pictures for the surface\n");
            return VA_STATUS_ERROR_UNKNOWN;
        }
        
        surface_subpic = ( PsbVASurfaceRec *)malloc(sizeof(*surface_subpic));
        if (NULL == surface_subpic)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        memset(surface_subpic,0,sizeof(*surface_subpic));
    }
    
    surface_subpic->subpic_id = obj_subpic->subpic_id;
    surface_subpic->fourcc = image->format.fourcc;
    surface_subpic->bo = obj_buffer->psb_buffer->drm_buf;
    surface_subpic->bufid = wsbmKBufHandle(wsbmKBuf(obj_buffer->psb_buffer->drm_buf));
    surface_subpic->pl_flags = obj_buffer->psb_buffer->pl_flags;
    
    surface_subpic->width = image->width;
    surface_subpic->height = image->height;
    switch (surface_subpic->fourcc) {
        case VA_FOURCC_AYUV:
            surface_subpic->stride = image->pitches[0]/4;
            break;
        case VA_FOURCC_RGBA:
            surface_subpic->stride = image->pitches[0]/4;
            break;
        case VA_FOURCC_AI44:
            surface_subpic->stride = image->pitches[0];
            /* point to Image palette */
            surface_subpic->palette_ptr = (PsbAYUVSample8 *)&obj_image->palette[0];
            break;
    }
    
    if (update) {
        surface_subpic->subpic_srcx = src_x;
        surface_subpic->subpic_srcy = src_y;
        surface_subpic->subpic_dstx = dest_x;
        surface_subpic->subpic_dsty = dest_y;
        surface_subpic->subpic_srcw = src_w;
        surface_subpic->subpic_srch = src_h;
        surface_subpic->subpic_dstw = dest_w;
        surface_subpic->subpic_dsth = dest_h;
    }
    
    if (found == 0) { /* new node, link into the list */
        if (NULL == obj_surface->subpictures) {
            obj_surface->subpictures = surface_subpic;
        } else { /* insert as the head */
            surface_subpic->next = obj_surface->subpictures;
            obj_surface->subpictures = surface_subpic;
        }
        obj_surface->subpic_count++;
    }
    
    return VA_STATUS_SUCCESS;
}


static VAStatus psb__LinkSurfaceIntoSubpict(
    object_subpic_p obj_subpic,
    VASurfaceID surface_id
)
{
    subpic_surface_s *subpic_surface;
    int found = 0;
    
    if (obj_subpic->surfaces != NULL) {
        subpic_surface = (subpic_surface_s *)obj_subpic->surfaces;
        do  {
            if (subpic_surface->surface_id == surface_id) {
                found = 1;
                return VA_STATUS_SUCCESS; /* reture directly */
            } else
                subpic_surface = subpic_surface->next;
        } while (subpic_surface);
    }

    /* not found */
    subpic_surface = (subpic_surface_s *)malloc(sizeof(*subpic_surface));
    if (NULL == subpic_surface)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    memset(subpic_surface,0,sizeof(*subpic_surface));

    subpic_surface->surface_id = surface_id;
    subpic_surface->next = NULL;
    
    if (NULL == obj_subpic->surfaces) {
        obj_subpic->surfaces = subpic_surface;
    } else { /* insert as the head */
        subpic_surface->next = obj_subpic->surfaces;
        obj_subpic->surfaces = subpic_surface;
    }
    
    return VA_STATUS_SUCCESS;
}

static VAStatus psb__DelinkSubpictFromSurface(
    object_surface_p obj_surface,
    VASubpictureID subpic_id
)
{
    PsbVASurfaceRec *surface_subpic,*pre_surface_subpic=NULL;
    int found = 0;
    
    if (obj_surface->subpictures != NULL) {
        surface_subpic = ( PsbVASurfaceRec *)obj_surface->subpictures;
        do  {
            if (surface_subpic->subpic_id == subpic_id) {
                found = 1;
                break;
            } else {
                pre_surface_subpic = surface_subpic;
                surface_subpic = surface_subpic->next;
            }
        } while (surface_subpic);
    }

    if (found == 1) {
        if (pre_surface_subpic == NULL) { /* remove the first node */
            obj_surface->subpictures = surface_subpic->next;
        } else {
            pre_surface_subpic->next = surface_subpic->next;
        }
        free(surface_subpic);
        obj_surface->subpic_count--;
    }
    
    return VA_STATUS_SUCCESS;
}


static VAStatus psb__DelinkSurfaceFromSubpict(
    object_subpic_p obj_subpic,
    VASurfaceID surface_id
)
{
    subpic_surface_s *subpic_surface,*pre_subpic_surface=NULL;
    int found = 0;
    
    if (obj_subpic->surfaces != NULL) {
        subpic_surface = (subpic_surface_s *)obj_subpic->surfaces;
        do {
            if (subpic_surface->surface_id == surface_id) {
                found = 1;
                break;
            } else {
                pre_subpic_surface = subpic_surface;
                subpic_surface = subpic_surface->next;
            }
        } while (subpic_surface);
    }

    if (found == 1) {
        if (pre_subpic_surface == NULL) { /* remove the first node */
            obj_subpic->surfaces = subpic_surface->next;
        } else {
            pre_subpic_surface->next = subpic_surface->next;
        }
        free(subpic_surface);
    }
    
    return VA_STATUS_SUCCESS;
}


VAStatus psb_QuerySubpictureFormats(
	VADriverContextP ctx,
	VAImageFormat *format_list,        /* out */
	unsigned int *flags,       /* out */
	unsigned int *num_formats  /* out */
)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    
    if(NULL == format_list)
    {
	vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
	DEBUG_FAILURE;
	return vaStatus;
    }
    if(NULL == flags)
    {
	vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
	DEBUG_FAILURE;
	return vaStatus;
    }
    if(NULL == num_formats)
    {
	vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
	DEBUG_FAILURE;
	return vaStatus;
    }
    memcpy(format_list,psb__SubpicFormat,sizeof(psb__SubpicFormat));
    *num_formats = PSB_MAX_SUBPIC_FORMATS;
    *flags = PSB_SUPPORTED_SUBPIC_FLAGS;

    return VA_STATUS_SUCCESS;
}


VAStatus psb_CreateSubpicture(
	VADriverContextP ctx,
	VAImageID image,
	VASubpictureID *subpicture   /* out */
)
{
    INIT_DRIVER_DATA;
    VASubpictureID subpicID;
    object_subpic_p obj_subpic;
    object_image_p obj_image;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    VAImageFormat *img_fmt;

    obj_image = IMAGE(image);

    if (NULL == subpicture)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SUBPICTURE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    if (NULL == obj_image)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_IMAGE;
        return vaStatus;
    }

    img_fmt = psb__VAImageCheckFourCC(&obj_image->image.format, psb__SubpicFormat,
                                      sizeof(psb__SubpicFormat)/sizeof(VAImageFormat));
    if (img_fmt == NULL)
        return VA_STATUS_ERROR_UNKNOWN;
    
    subpicID = object_heap_allocate( &driver_data->subpic_heap );
    obj_subpic = SUBPIC(subpicID);
    if (NULL == obj_subpic)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        return vaStatus;
    }
    obj_subpic->subpic_id = subpicID;
    obj_subpic->image_id = obj_image->image.image_id;
    obj_subpic->surfaces = NULL;

    obj_image->subpic_ref ++;
    
    *subpicture = subpicID;
    
    return VA_STATUS_SUCCESS;
}



VAStatus psb__destroy_subpicture(psb_driver_data_p driver_data, object_subpic_p obj_subpic)
{
    subpic_surface_s *subpic_surface = (subpic_surface_s *)obj_subpic->surfaces;
    VASubpictureID subpicture = obj_subpic->subpic_id;

    if (subpic_surface) {
        do {
            subpic_surface_s *tmp = subpic_surface;
            object_surface_p obj_surface = SURFACE(subpic_surface->surface_id);
            
            if (obj_surface) { /* remove subpict from surface */
                psb__DelinkSubpictFromSurface(obj_surface,subpicture);
            }
            subpic_surface = subpic_surface->next;
            free(tmp);
        } while (subpic_surface);
    }

    object_heap_free( &driver_data->subpic_heap, (object_base_p) obj_subpic);
    return VA_STATUS_SUCCESS;
}


VAStatus psb_DestroySubpicture(
	VADriverContextP ctx,
	VASubpictureID subpicture
)
{
    INIT_DRIVER_DATA;
    object_subpic_p obj_subpic;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    
    obj_subpic = SUBPIC(subpicture);
    if (NULL == obj_subpic)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SUBPICTURE;
        return vaStatus;
    }

    
    return psb__destroy_subpicture(driver_data, obj_subpic);
}

VAStatus psb_SetSubpictureImage (
	VADriverContextP ctx,
        VASubpictureID subpicture,
        VAImageID image
)
{
   INIT_DRIVER_DATA;
    object_subpic_p obj_subpic;
    object_image_p obj_image;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    subpic_surface_s *subpic_surface;
    VAImageFormat *img_fmt;

    obj_image = IMAGE(image);
    if (NULL == obj_image)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_IMAGE;
        return vaStatus;
    }

    img_fmt = psb__VAImageCheckFourCC(&obj_image->image.format,
                                      psb__SubpicFormat,
                                      sizeof(psb__SubpicFormat)/sizeof(VAImageFormat));
    if (img_fmt == NULL)
        return VA_STATUS_ERROR_INVALID_IMAGE;
    
    obj_subpic = SUBPIC(subpicture);
    if (NULL == obj_subpic)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SUBPICTURE;
        return vaStatus;
    }

    object_image_p old_obj_image = IMAGE(obj_subpic->image_id);
    if (old_obj_image) {
        old_obj_image->subpic_ref--;/* decrease reference count */
    }

    /* reset the image */
    obj_subpic->image_id = obj_image->image.image_id;
    obj_image->subpic_ref ++;
    
    /* relink again */
    if (obj_subpic->surfaces != NULL) {
        /* the subpicture already linked into surfaces
         * so not check the return value of psb__LinkSubpictIntoSurface
         */
        subpic_surface = (subpic_surface_s *)obj_subpic->surfaces;
        do {
            object_surface_p obj_surface = SURFACE(subpic_surface->surface_id);
            if (NULL == obj_surface)
            {
                vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
                return vaStatus;
            }

            psb__LinkSubpictIntoSurface(driver_data,obj_surface,obj_subpic,
                                        0,0,0,0,0,0,0,0,
                                        0 /* not update location */
                                        );
            subpic_surface = subpic_surface->next;
        } while (subpic_surface);
    }

       
    return VA_STATUS_SUCCESS;
}


VAStatus psb_SetSubpictureChromakey(
	VADriverContextP ctx,
	VASubpictureID subpicture,
	unsigned int chromakey_min,
	unsigned int chromakey_max,
	unsigned int chromakey_mask
)
{
    INIT_DRIVER_DATA;
    (void)driver_data;    
    /* TODO */
    return VA_STATUS_ERROR_UNKNOWN;
}

VAStatus psb_SetSubpictureGlobalAlpha(
	VADriverContextP ctx,
	VASubpictureID subpicture,
	float global_alpha 
)
{
    INIT_DRIVER_DATA;
    (void)driver_data;    
    /* TODO */
    return VA_STATUS_ERROR_UNKNOWN;
}


VAStatus psb__AssociateSubpicture(
	VADriverContextP ctx,
	VASubpictureID subpicture,
	VASurfaceID *target_surfaces,
	int num_surfaces,
	short src_x, /* upper left offset in subpicture */
	short src_y,
	unsigned short src_w,
	unsigned short src_h,
        short dest_x, /* upper left offset in surface */
	short dest_y,
	unsigned short dest_w,
	unsigned short dest_h,
	/*
	 * whether to enable chroma-keying or global-alpha
	 * see VA_SUBPICTURE_XXX values
	 */
	unsigned int flags
)
{
    INIT_DRIVER_DATA;

    object_subpic_p obj_subpic;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int i;

    if (num_surfaces <= 0)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
        DEBUG_FAILURE;
        return vaStatus;
    }

    obj_subpic = SUBPIC(subpicture);
    if (NULL == obj_subpic)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SUBPICTURE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    if (NULL == target_surfaces)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }
    
    if (flags & ~PSB_SUPPORTED_SUBPIC_FLAGS)
    {
#ifdef VA_STATUS_ERROR_FLAG_NOT_SUPPORTED
        vaStatus = VA_STATUS_ERROR_FLAG_NOT_SUPPORTED;
#else
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
#endif        
        DEBUG_FAILURE;
        return vaStatus;
    }

    /* Validate input params */
    for (i=0; i<num_surfaces;i++) {
        object_surface_p obj_surface = SURFACE(target_surfaces[i]);
        if (NULL == obj_surface) {
            vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
            DEBUG_FAILURE;
            return vaStatus;
        }
    }

    VASurfaceID *surfaces=target_surfaces;
    for (i=0; i<num_surfaces;i++) {
        object_surface_p obj_surface = SURFACE(*surfaces);
        if (obj_surface) {
            vaStatus = psb__LinkSubpictIntoSurface(driver_data,obj_surface,obj_subpic,
                                                   src_x,src_y,src_w,src_h,
                                                   dest_x,dest_y,dest_w,dest_h,1);
            if (VA_STATUS_SUCCESS == vaStatus) { 
                vaStatus = psb__LinkSurfaceIntoSubpict(obj_subpic,*surfaces);
            }
            if (VA_STATUS_SUCCESS != vaStatus) { /* failed with malloc */
                DEBUG_FAILURE;
                return vaStatus;
            }
        } else {
            /* Should never get here */
            psb__error_message("Invalid surfaces,SurfaceID=0x%x\n",*surfaces);
        }
        
        surfaces++;
    }
    
    return VA_STATUS_SUCCESS;
}


VAStatus psb_AssociateSubpicture(
	VADriverContextP ctx,
	VASubpictureID subpicture,
	VASurfaceID *target_surfaces,
	int num_surfaces,
	short src_x, /* upper left offset in subpicture */
	short src_y,
	unsigned short src_width,
	unsigned short src_height,
	short dest_x, /* upper left offset in surface */
	short dest_y,
	unsigned short dest_width,
	unsigned short dest_height,
	/*
	 * whether to enable chroma-keying or global-alpha
	 * see VA_SUBPICTURE_XXX values
	 */
	unsigned int flags
)
{
    return psb__AssociateSubpicture(ctx,subpicture,target_surfaces,num_surfaces,
                                    src_x,src_y,src_width,src_height,
                                    dest_x,dest_y,dest_width,dest_height,
                                    flags
                                    );
}


VAStatus psb_DeassociateSubpicture(
	VADriverContextP ctx,
	VASubpictureID subpicture,
	VASurfaceID *target_surfaces,
	int num_surfaces
)
{
    INIT_DRIVER_DATA;

    object_subpic_p obj_subpic;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_image_p obj_image;
    int i;

    if (num_surfaces <= 0)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
        DEBUG_FAILURE;
        return vaStatus;
    }
    
    obj_subpic = SUBPIC(subpicture);
    if (NULL == obj_subpic)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SUBPICTURE;
        DEBUG_FAILURE;
        return vaStatus;
    }
    
    if (NULL == target_surfaces)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    VASurfaceID *surfaces=target_surfaces;
    for (i=0; i<num_surfaces;i++) {
        object_surface_p obj_surface = SURFACE(*surfaces);
        
        if (obj_surface) {
            psb__DelinkSubpictFromSurface(obj_surface,subpicture);
            psb__DelinkSurfaceFromSubpict(obj_subpic,obj_surface->surface_id);
        } else {
            psb__error_message("vaDeassociateSubpicture: Invalid surface, VASurfaceID=0x%08x\n",*surfaces);
        }
        
        surfaces++;
    }

    obj_image = IMAGE(obj_subpic->image_id);
    if (obj_image)
        obj_image->subpic_ref--;/* decrease reference count */
    
    return VA_STATUS_SUCCESS;
}


void psb_SurfaceDeassociateSubpict(
    psb_driver_data_p driver_data,
    object_surface_p obj_surface)
{
     PsbVASurfaceRec *surface_subpic = ( PsbVASurfaceRec *)obj_surface->subpictures;

    if (surface_subpic != NULL) {
        do  {
            PsbVASurfaceRec *tmp = surface_subpic;
            object_subpic_p obj_subpic = SUBPIC(surface_subpic->subpic_id);
            if (obj_subpic)
                psb__DelinkSurfaceFromSubpict(obj_subpic,obj_surface->surface_id);
            surface_subpic = surface_subpic->next;
            free(tmp);
        } while (surface_subpic);
    }
}

        
static  VADisplayAttribute psb__DisplayAttribute[] = {
    {
        VADisplayAttribBrightness,
        BRIGHTNESS_MIN,
        BRIGHTNESS_MAX,
        BRIGHTNESS_DEFAULT_VALUE,
        VA_DISPLAY_ATTRIB_GETTABLE|VA_DISPLAY_ATTRIB_SETTABLE
    },

    {
        VADisplayAttribContrast,
        CONTRAST_MIN,
        CONTRAST_MAX,
        CONTRAST_DEFAULT_VALUE,
        VA_DISPLAY_ATTRIB_GETTABLE|VA_DISPLAY_ATTRIB_SETTABLE
    },

    {
        VADisplayAttribHue,
        HUE_MIN,
        HUE_MAX,
        HUE_DEFAULT_VALUE,
        VA_DISPLAY_ATTRIB_GETTABLE|VA_DISPLAY_ATTRIB_SETTABLE
    },

    {
        VADisplayAttribSaturation,
        SATURATION_MIN,
        SATURATION_MAX,
        SATURATION_DEFAULT_VALUE,
        VA_DISPLAY_ATTRIB_GETTABLE|VA_DISPLAY_ATTRIB_SETTABLE
    },
    {
        VADisplayAttribBackgroundColor,
        0x00000000,
        0xffffffff,
        0x00000000,
        VA_DISPLAY_ATTRIB_GETTABLE|VA_DISPLAY_ATTRIB_SETTABLE
    },
};

/* 
 * Query display attributes 
 * The caller must provide a "attr_list" array that can hold at
 * least vaMaxNumDisplayAttributes() entries. The actual number of attributes
 * returned in "attr_list" is returned in "num_attributes".
 */
VAStatus psb_QueryDisplayAttributes (
		VADriverContextP ctx,
		VADisplayAttribute *attr_list,	/* out */
		int *num_attributes		/* out */
	)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    if(NULL == attr_list)
    {
	vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
	DEBUG_FAILURE;
	return vaStatus;
    }
    if(NULL == num_attributes)
    {
	vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
	DEBUG_FAILURE;
	return vaStatus;
    }
    *num_attributes = min(*num_attributes,PSB_MAX_DISPLAY_ATTRIBUTES);
    memcpy(attr_list,psb__DisplayAttribute,*num_attributes);

    return VA_STATUS_SUCCESS;
}

/* 
 * Get display attributes 
 * This function returns the current attribute values in "attr_list".
 * Only attributes returned with VA_DISPLAY_ATTRIB_GETTABLE set in the "flags" field
 * from vaQueryDisplayAttributes() can have their values retrieved.  
 */
VAStatus psb_GetDisplayAttributes (
		VADriverContextP ctx,
		VADisplayAttribute *attr_list,	/* in/out */
		int num_attributes
)
{
    psb_output_p output = GET_OUTPUT_DATA(ctx);
    VADisplayAttribute *p=attr_list;
    int i;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    
    if(NULL == attr_list)
    {
	vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
	DEBUG_FAILURE;
	return vaStatus;
    }

    if (num_attributes <= 0)
    {
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    for (i=0;i<num_attributes;i++) {
        switch (p->type) {
            case VADisplayAttribBrightness:
                p->value = output->brightness.Value;
                break;
            case VADisplayAttribContrast:
                p->value = output->contrast.Value;
                break;
            case VADisplayAttribHue:
                p->value = output->hue.Value;
                break;
            case VADisplayAttribSaturation:
                p->value = output->saturation.Value;
                break;
            case VADisplayAttribBackgroundColor:
                p->value = output->clear_color;
                break;
            default:
                break;
        }
        p++;
    }

    return VA_STATUS_SUCCESS;
}

/* 
 * Set display attributes 
 * Only attributes returned with VA_DISPLAY_ATTRIB_SETTABLE set in the "flags" field
 * from vaQueryDisplayAttributes() can be set.  If the attribute is not settable or 
 * the value is out of range, the function returns VA_STATUS_ERROR_ATTR_NOT_SUPPORTED
 */
#define CLAMP_ATTR(a,max,min) (a>max?max:(a<min?min:a))
VAStatus psb_SetDisplayAttributes (
		VADriverContextP ctx,
		VADisplayAttribute *attr_list,
		int num_attributes
	)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    
    if(NULL == attr_list)
    {
	vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
	DEBUG_FAILURE;
	return vaStatus;
    }
    psb_output_p output = GET_OUTPUT_DATA(ctx);
    VADisplayAttribute *p=attr_list;
    int i,update_coeffs=0;

    if (num_attributes <= 0)
    {
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    for (i=0;i<num_attributes;i++) {
        switch (p->type) {
            case VADisplayAttribBrightness:
                output->brightness.Value = CLAMP_ATTR(p->value,BRIGHTNESS_MAX,BRIGHTNESS_MIN);
                update_coeffs = 1;
                break;
            case VADisplayAttribContrast:
                output->contrast.Value = CLAMP_ATTR(p->value,CONTRAST_MAX,CONTRAST_MIN);
                update_coeffs = 1;
                break;
            case VADisplayAttribHue:
                output->hue.Value = CLAMP_ATTR(p->value,HUE_MAX,HUE_MIN);
                update_coeffs = 1;
                break;
            case VADisplayAttribSaturation:
                output->saturation.Value = CLAMP_ATTR(p->value,SATURATION_MAX,SATURATION_MIN);
                update_coeffs = 1;
                break;
            case VADisplayAttribBackgroundColor:
                output->clear_color = p->value;
                break;
            default:
                break;
        }
        p++;
    }

    if (update_coeffs) {
        /* TODO */
    }

    return VA_STATUS_SUCCESS;
}
