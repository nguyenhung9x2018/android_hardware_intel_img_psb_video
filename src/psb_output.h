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

#ifndef _PSB_OUTPUT_H_
#define _PSB_OUTPUT_H_
#include <inttypes.h>
#include "psb_drv_video.h"
#include "psb_xvva.h"
#include "psb_drm.h"
#include "psb_surface.h"
#include "hwdefs/img_types.h"
#include <va/va.h>

#ifndef ANDROID
#include <va/va_x11.h>
#else
#define Drawable unsigned int
#define Bool int
#define LOG_TAG "pvr_drv_video"
#include <cutils/log.h>
#endif

#define ANDROID_VIDEO_TEXTURE_STREAM 1

#define PSB_MAX_IMAGE_FORMATS      4 /* sizeof(psb__CreateImageFormat)/sizeof(VAImageFormat) */
#define PSB_MAX_SUBPIC_FORMATS     3 /* sizeof(psb__SubpicFormat)/sizeof(VAImageFormat) */
#define PSB_MAX_DISPLAY_ATTRIBUTES 6 /* sizeof(psb__DisplayAttribute)/sizeof(VADisplayAttribute) */

#define PSB_SUPPORTED_SUBPIC_FLAGS	0 /* No alpha or chroma key support */

#define GET_OUTPUT_DATA(ctx) (psb_output_p)(((psb_driver_data_p)ctx->pDriverData)->video_output)
#define CLAMP(_X) ( (_X)= ((_X)<0?0:((_X)>255?255:(_X)) ) )

#define HUE_DEFAULT_VALUE    0
#define HUE_MIN    -180
#define HUE_MAX    180
#define HUE_STEPSIZE 0.1

#define BRIGHTNESS_DEFAULT_VALUE   0
#define BRIGHTNESS_MIN -100
#define BRIGHTNESS_MAX 100
#define BRIGHTNESS_STEPSIZE 0.1

#define CONTRAST_DEFAULT_VALUE 1
#define CONTRAST_MIN 0
#define CONTRAST_MAX 2
#define CONTRAST_STEPSIZE 0.0001

#define SATURATION_DEFAULT_VALUE 1
#define SATURATION_MIN 0
#define SATURATION_MAX 3
#define SATURATION_STEPSIZE 0.0001

#define PSB_DRIDDX_VERSION_MAJOR 0
#define PSB_DRIDDX_VERSION_MINOR 1
#define PSB_DRIDDX_VERSION_PATCH 0

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
typedef struct _psb_fixed32 {
    union {
        struct {
            unsigned short      Fraction;
            short       Value;
        };
        long ll;
    };
} psb_fixed32;

/* surfaces link list associated with a subpicture */
typedef struct _subpic_surface {
    VASurfaceID surface_id;
    struct _subpic_surface *next;
} subpic_surface_s, *subpic_surface_p;

#define USING_OVERLAY_PORT  1
#define USING_TEXTURE_PORT  2

typedef struct _psb_output_s {
    enum {
        PSB_PUTSURFACE_NONE = 0,
        PSB_PUTSURFACE_X11,/* use x11 method */
	PSB_PUTSURFACE_TEXTURE_XV,/* force texture xvideo */
	PSB_PUTSURFACE_OVERLAY_XV,/* force overlay xvideo */
	PSB_PUTSURFACE_FORCE_TEXTURE_XV,/* force texture xvideo */
	PSB_PUTSURFACE_FORCE_OVERLAY_XV,/* force overlay xvideo */
    } output_method;

    /*information for xvideo*/
    XvPortID 			textured_portID;
    XvPortID 			overlay_portID;
    XvImage *			textured_xvimage;
    XvImage *			overlay_xvimage;
    PsbXvVAPutSurfaceRec        imgdata_vasrf;
    GC				gc;
    Drawable                    output_drawable;
    unsigned short              output_width;
    unsigned short              output_height;

    int                         using_port;
    int                         is_oold;
    
    /* information of display attribute */
    psb_fixed32 brightness;
    psb_fixed32 contrast;
    psb_fixed32 saturation;
    psb_fixed32 hue;


    /* subpic number current buffers support */
    unsigned int max_subpic;
    unsigned int clear_color;
    
    /*for multi-thread safe*/
    pthread_mutex_t output_mutex;

    int drawable_info;
    int ignore_dpm;
    int dummy_putsurface;
    int fixed_fps;
   
    /* for android surface register */
    int register_flag;

    /* for memory heap base used by putsurface*/
    unsigned char* heap_addr;

    /*for video rotation with overlay adaptor*/
    psb_surface_p rotate_surface;
    int rotate_surfaceID;
    unsigned int sprite_enabled;

    unsigned int frame_count;
} psb_output_s, *psb_output_p;

VAStatus psb__destroy_subpicture(psb_driver_data_p driver_data, object_subpic_p obj_subpic);
VAStatus psb__destroy_image(psb_driver_data_p driver_data, object_image_p obj_image);

/*
 * VAImage call these buffer routines
 */
VAStatus psb__CreateBuffer(
	psb_driver_data_p driver_data,
	object_context_p obj_context,   /* in */
	VABufferType type,      /* in */
	unsigned int size,      /* in */
	unsigned int num_elements, /* in */
	void *data,             /* in */
	VABufferID *buf_desc    /* out */
);

VAStatus psb_DestroyBuffer(
        VADriverContextP ctx,
        VABufferID buffer_id
);

VAStatus psb_initOutput(
    VADriverContextP ctx
);


VAStatus psb_deinitOutput(
    VADriverContextP ctx
);

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
);

VAStatus psb_PutSurface(
        VADriverContextP ctx,
        VASurfaceID surface,
        void* draw, /* X Drawable */
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
);

VAStatus psb_QueryImageFormats(
	VADriverContextP ctx,
	VAImageFormat *format_list,        /* out */
	int *num_formats           /* out */
);

VAStatus psb_CreateImage(
	VADriverContextP ctx,
	VAImageFormat *format,
	int width,
	int height,
	VAImage *image     /* out */
);

VAStatus psb_DeriveImage(
	VADriverContextP ctx,
	VASurfaceID surface,
	VAImage *image     /* out */
);

VAStatus psb_DestroyImage(
	VADriverContextP ctx,
	VAImageID image
);

VAStatus psb_SetImagePalette (
	VADriverContextP ctx,
	VAImageID image,
	/* 
	 * pointer to an array holding the palette data.  The size of the array is
	 * num_palette_entries * entry_bytes in size.  The order of the components 
	 * in the palette is described by the component_order in VAImage struct
	 */
	unsigned char *palette 
);

VAStatus psb_GetImage(
	VADriverContextP ctx,
	VASurfaceID surface,
	int x,     /* coordinates of the upper left source pixel */
	int y,
	unsigned int width, /* width and height of the region */
	unsigned int height,
	VAImageID image
);

VAStatus psb_PutImage(
	VADriverContextP ctx,
	VASurfaceID surface,
	VAImageID image,
	int src_x,
	int src_y,
	unsigned int src_width,
	unsigned int src_height,
	int dest_x,
	int dest_y,
	unsigned int dest_width,
	unsigned int dest_height
);

VAStatus psb_QuerySubpictureFormats(
	VADriverContextP ctx,
	VAImageFormat *format_list,        /* out */
	unsigned int *flags,       /* out */
	unsigned int *num_formats  /* out */
);

VAStatus psb_CreateSubpicture(
	VADriverContextP ctx,
	VAImageID image,
	VASubpictureID *subpicture   /* out */
);

VAStatus psb_DestroySubpicture(
	VADriverContextP ctx,
	VASubpictureID subpicture
);

VAStatus psb_SetSubpictureImage (
	VADriverContextP ctx,
        VASubpictureID subpicture,
        VAImageID image
);


VAStatus psb_SetSubpictureChromakey(
	VADriverContextP ctx,
	VASubpictureID subpicture,
	unsigned int chromakey_min,
	unsigned int chromakey_max,
	unsigned int chromakey_mask
);

VAStatus psb_SetSubpictureGlobalAlpha(
	VADriverContextP ctx,
	VASubpictureID subpicture,
	float global_alpha 
);

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
);

VAStatus psb_DeassociateSubpicture(
	VADriverContextP ctx,
	VASubpictureID subpicture,
	VASurfaceID *target_surfaces,
	int num_surfaces
);

void psb_SurfaceDeassociateSubpict(
    psb_driver_data_p driver_data,
    object_surface_p obj_surface
);

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
);

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
);

/* 
 * Set display attributes 
 * Only attributes returned with VA_DISPLAY_ATTRIB_SETTABLE set in the "flags" field
 * from vaQueryDisplayAttributes() can be set.  If the attribute is not settable or 
 * the value is out of range, the function returns VA_STATUS_ERROR_ATTR_NOT_SUPPORTED
 */
VAStatus psb_SetDisplayAttributes (
		VADriverContextP ctx,
		VADisplayAttribute *attr_list,
		int num_attributes
);

VAStatus psb_putsurface_xvideo(
    VADriverContextP ctx,
    VASurfaceID surface,
    Drawable draw,
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
);

VAStatus psb_init_xvideo(VADriverContextP ctx);
VAStatus psb_deinit_xvideo(VADriverContextP ctx); 

/*add for texture streaming*/
#define DRM_BUFFER_CLASS_VIDEO          0x2C

#define BC_FOURCC(a,b,c,d) \
    ((unsigned long) ((a) | (b)<<8 | (c)<<16 | (d)<<24))

#define BC_PIX_FMT_NV12     BC_FOURCC('N', 'V', '1', '2') /*YUV 4:2:0*/
#define BC_PIX_FMT_UYVY     BC_FOURCC('U', 'Y', 'V', 'Y') /*YUV 4:2:2*/
#define BC_PIX_FMT_YUYV     BC_FOURCC('Y', 'U', 'Y', 'V') /*YUV 4:2:2*/
#define BC_PIX_FMT_RGB565   BC_FOURCC('R', 'G', 'B', 'P') /*RGB 5:6:5*/

typedef struct BC_Video_ioctl_package_TAG
{
	int ioctl_cmd;
	int inputparam;
	int outputparam;
}BC_Video_ioctl_package;

typedef struct bc_buf_ptr {
    unsigned int index;
    int size;
    unsigned long pa;
    unsigned long handle;
} bc_buf_ptr_t;

#define BC_Video_ioctl_fill_buffer		    0
#define BC_Video_ioctl_get_buffer_count	    1
#define BC_Video_ioctl_get_buffer_phyaddr   2  /*get physical address by index*/
#define BC_Video_ioctl_get_buffer_index 	3  /*get index by physical address*/
#define BC_Video_ioctl_request_buffers      4
#define BC_Video_ioctl_set_buffer_phyaddr   5

enum BC_memory {
    BC_MEMORY_MMAP          = 1,
    BC_MEMORY_USERPTR       = 2,
};

/* 
 * the following types are tested for fourcc in struct bc_buf_params_t
 *   NV12
 *   UYVY
 *   RGB565 - not tested yet
 *   YUYV
 */
typedef struct bc_buf_params {
    int count;              /*number of buffers, [in/out]*/
    int width;              /*buffer width in pixel, multiple of 8 or 32*/
    int height;             /*buffer height in pixel*/
    int stride;
    unsigned int fourcc;    /*buffer pixel format*/
    enum BC_memory type;
} bc_buf_params_t;

/*add for texture streaming end*/

#endif /* _PSB_OUTPUT_H_ */
