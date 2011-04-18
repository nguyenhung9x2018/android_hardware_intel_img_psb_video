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


/*
 * Authors:
 *    Shengquan Yuan  <shengquan.yuan@intel.com>
 *    Zhaohan Ren  <zhaohan.ren@intel.com>
 *
 */



#ifndef _PSB_X11_H_
#define _PSB_X11_H_

#include <X11/Xutil.h>

#include <inttypes.h>
#include "psb_drv_video.h"
#include "psb_drm.h"
#include "psb_surface.h"
#include "psb_output.h"
#include "psb_surface_ext.h"

#include <va/va.h>

#define USING_OVERLAY_PORT  1
#define USING_TEXTURE_PORT  2

typedef struct {
    /*src coordinate*/
    short srcx;
    short srcy;
    unsigned short sWidth;
    unsigned short sHeight;
    /*dest coordinate*/
    short destx;
    short desty;
    unsigned short dWidth;
    unsigned short dHeight;
} psb_overlay_rect_t, *psb_overlay_rect_p;

typedef struct {
    int                 i32Left;
    int                 i32Top;
    int                 i32Right;
    int                 i32Bottom;
    unsigned int        ui32Width;
    unsigned int        ui32Height;
} psb_x11_win_t;

typedef struct x11_rect_list {
    psb_x11_win_t     rect;
    struct x11_rect_list * next;
} psb_x11_clip_list_t;

typedef struct _psb_x11_output_s {
    /* information for xvideo */
    XvPortID textured_portID;
    XvPortID overlay_portID;
    XvImage *textured_xvimage;
    XvImage *overlay_xvimage;
    PsbXvVAPutSurfaceRec        imgdata_vasrf;
    GC                          gc;
    Drawable                    output_drawable;
    int                         is_pixmap;
    Drawable                    output_drawable_save;
    GC                          extend_gc;
    Drawable                    extend_drawable;
    unsigned short              output_width;
    unsigned short              output_height;
    int                         using_port;

    int bIsVisible;
    psb_x11_win_t winRect;
    psb_x11_clip_list_t *pClipBoxList;
    unsigned int ui32NumClipBoxList;
    unsigned int frame_count;

    int ignore_dpm;

    /*for video rotation with overlay adaptor*/
    psb_surface_p rotate_surface;
    int rotate_surfaceID;
    int rotate;
    unsigned int sprite_enabled;

} psb_x11_output_s, *psb_x11_output_p;

VAStatus psb_putsurface_coverlay(
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

VAStatus psb_putsurface_ctexture(
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
    unsigned int flags /* de-interlacing flags */
);

VAStatus psb_init_xvideo(VADriverContextP ctx, psb_x11_output_p output);
VAStatus psb_deinit_xvideo(VADriverContextP ctx);

#endif
