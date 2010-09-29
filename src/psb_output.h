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

#ifndef _PSB_OUTPUT_H_
#define _PSB_OUTPUT_H_
#include <inttypes.h>
#include "psb_drv_video.h"
#include "psb_drm.h"
#include "psb_surface.h"
#include "hwdefs/img_types.h"
#include <va/va.h>
#include <linux/fb.h>
#include <fcntl.h>

#ifndef ANDROID
#include <va/va_x11.h>
#else
#define Drawable unsigned int
#define Bool int
#define LOG_TAG "pvr_drv_video"
#endif

#define PSB_MAX_IMAGE_FORMATS      5 /* sizeof(psb__CreateImageFormat)/sizeof(VAImageFormat) */
#define PSB_MAX_SUBPIC_FORMATS     3 /* sizeof(psb__SubpicFormat)/sizeof(VAImageFormat) */
#define PSB_MAX_DISPLAY_ATTRIBUTES 6 /* sizeof(psb__DisplayAttribute)/sizeof(VADisplayAttribute) */

#define PSB_SUPPORTED_SUBPIC_FLAGS	0 /* No alpha or chroma key support */


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

#define psb__ImageYV16                          \
{                                               \
    VA_FOURCC_YV16,                             \
    VA_LSB_FIRST,                               \
    16,                                         \
    0,                                          \
    0,                                          \
    0,                                          \
    0,                                          \
    0,                                          \
}

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

#endif /* _PSB_OUTPUT_H_ */
