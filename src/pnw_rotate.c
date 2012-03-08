/*
 * Copyright (c) 2011 Intel Corporation. All Rights Reserved.
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
 *
 * Authors:
 *    Zeng Li <zeng.li@intel.com>
 *    Jason Hu <jason.hu@intel.com>
 *    Shengquan Yuan  <shengquan.yuan@intel.com>
 */

#include <va/va.h>
#include <va/va_backend.h>
#include <va/va_backend_tpi.h>
#include <va/va_backend_egl.h>
#include <va/va_dricommon.h>

#include "psb_drv_video.h"
#include "psb_output.h"
#include "vc1_defs.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <wsbm/wsbm_pool.h>
#include <wsbm/wsbm_manager.h>
#include <wsbm/wsbm_util.h>
#include <wsbm/wsbm_fencemgr.h>
#include <system/graphics.h>

#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData
#define SURFACE(id)    ((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))

#define CHECK_SURFACE_REALLOC(psb_surface, msvdx_rotate, need)  \
do {                                                            \
    int old_rotate = GET_SURFACE_INFO_rotate(psb_surface);      \
    switch (msvdx_rotate) {                                     \
    case 2: /* 180 */                                           \
        if (old_rotate == 2)                                  \
            need = 0;                                           \
        else                                                    \
            need = 1;                                           \
        break;                                                  \
    case 1: /* 90 */                                            \
    case 3: /* 270 */                                           \
        if (old_rotate == 1 || old_rotate == 3)                 \
            need = 0;                                           \
        else                                                    \
            need = 1;                                           \
        break;                                                  \
    }                                                           \
} while (0)

//#define OVERLAY_ENABLE_MIRROR

static uint32_t VAROTATION2HAL(int va_rotate) {
        switch (va_rotate) {
        case VA_ROTATION_90:
            return HAL_TRANSFORM_ROT_90;
        case VA_ROTATION_180:
            return HAL_TRANSFORM_ROT_180;
        case VA_ROTATION_270:
            return HAL_TRANSFORM_ROT_270;
        default:
            return 0;
        }
}

void psb_InitRotate(VADriverContextP ctx)
{
    char env_value[64];
    INIT_DRIVER_DATA;

    /* VA rotate from APP */
    driver_data->va_rotate = VA_ROTATION_NONE;

    /* window manager rotation from OS */
    driver_data->mipi0_rotation = VA_ROTATION_NONE;
    driver_data->mipi1_rotation = VA_ROTATION_NONE;
    driver_data->hdmi_rotation = VA_ROTATION_NONE;

    /* final rotation of VA rotate+WM rotate */
    driver_data->local_rotation = VA_ROTATION_NONE;
    driver_data->extend_rotation = VA_ROTATION_NONE;

    /* MSVDX rotate */
    driver_data->msvdx_rotate_want = ROTATE_VA2MSVDX(VA_ROTATION_NONE);

    if (psb_parse_config("PSB_VIDEO_NOROTATE", &env_value[0]) == 0) {
        psb__information_message("MSVDX: disable MSVDX rotation\n");
        driver_data->disable_msvdx_rotate = 1;
    }
}

void psb_RecalcRotate(VADriverContextP ctx, object_context_p obj_context)
{
    INIT_DRIVER_DATA;
    int angle, new_rotate, i;
    int old_rotate = driver_data->msvdx_rotate_want;

    if (psb_android_is_extvideo_mode() || (driver_data->va_rotate != 0)) {
        if (driver_data->mipi0_rotation != 0) {
            driver_data->mipi0_rotation = 0;
            driver_data->hdmi_rotation = 0;
        }
    } else {
        int display_rotate = 0;
        psb_android_surfaceflinger_rotate(driver_data->native_window, &display_rotate);
        psb__information_message("NativeWindow(0x%x), get surface flinger rotate %d\n", driver_data->native_window, display_rotate);

        if (driver_data->mipi0_rotation != display_rotate) {
            driver_data->mipi0_rotation = display_rotate;
        }
    }

    /* calc VA rotation and WM rotation, and assign to the final rotation degree */
    angle = Rotation2Angle(driver_data->va_rotate) + Rotation2Angle(driver_data->mipi0_rotation);
    driver_data->local_rotation = Angle2Rotation(angle);
    angle = Rotation2Angle(driver_data->va_rotate) + Rotation2Angle(driver_data->hdmi_rotation);
    driver_data->extend_rotation = Angle2Rotation(angle);

    /* for any case that local and extened rotation are not same, fallback to GPU */
    if ((driver_data->mipi1_rotation != VA_ROTATION_NONE) ||
        ((driver_data->local_rotation != VA_ROTATION_NONE) &&
         (driver_data->extend_rotation != VA_ROTATION_NONE) &&
         (driver_data->local_rotation != driver_data->extend_rotation))) {
        new_rotate = ROTATE_VA2MSVDX(driver_data->local_rotation);
        if (driver_data->is_android == 0) /*fallback to texblit path*/
            driver_data->output_method = PSB_PUTSURFACE_CTEXTURE;
    } else {
        if (driver_data->local_rotation == VA_ROTATION_NONE)
            new_rotate = driver_data->extend_rotation;
        else
            new_rotate = driver_data->local_rotation;

        if (driver_data->is_android == 0) {
            if (driver_data->output_method != PSB_PUTSURFACE_FORCE_CTEXTURE)
                driver_data->output_method = PSB_PUTSURFACE_COVERLAY;
        }
    }

    if (old_rotate != new_rotate) {
        for (i = 0; i < obj_context->num_render_targets; i++) {
            object_surface_p obj_surface = SURFACE(obj_context->render_targets[i]);
            /*we invalidate all surfaces's rotate buffer share info here.*/
            if (obj_surface->share_info) {
                obj_surface->share_info->surface_rotate = 0;
                obj_surface->share_info->metadata_rotate = VAROTATION2HAL(driver_data->va_rotate);
            }
        }
        psb__information_message("MSVDX: new rotation %d desired\n", new_rotate);
        driver_data->msvdx_rotate_want = new_rotate;
    }

}


void psb_CheckInterlaceRotate(object_context_p obj_context, unsigned char *pic_param_tmp)
{
    int interaced_stream;
    object_surface_p obj_surface = obj_context->current_render_target;

    switch (obj_context->profile) {
    case VAProfileMPEG2Simple:
    case VAProfileMPEG2Main:
        break;
    case VAProfileMPEG4Simple:
    case VAProfileMPEG4AdvancedSimple:
    case VAProfileMPEG4Main:
    case VAProfileH263Baseline: {
        VAPictureParameterBufferMPEG4 *pic_params = (VAPictureParameterBufferMPEG4 *)pic_param_tmp;

        if (pic_params->vol_fields.bits.interlaced)
            obj_context->interlaced_stream = 1; /* is it the right way to check? */
        break;
    }
    case VAProfileH264Baseline:
    case VAProfileH264Main:
    case VAProfileH264High:
    case VAProfileH264ConstrainedBaseline: {
        VAPictureParameterBufferH264 *pic_params = (VAPictureParameterBufferH264 *)pic_param_tmp;
        /* is it the right way to check? */
        if (pic_params->pic_fields.bits.field_pic_flag || pic_params->seq_fields.bits.mb_adaptive_frame_field_flag)
            obj_context->interlaced_stream = 1;

        break;
    }
    case VAProfileVC1Simple:
    case VAProfileVC1Main:
    case VAProfileVC1Advanced: {
        VAPictureParameterBufferVC1 *pic_params = (VAPictureParameterBufferVC1 *)pic_param_tmp;

        /* is it the right way to check? */
        if (pic_params->sequence_fields.bits.interlace && (pic_params->picture_fields.bits.frame_coding_mode == VC1_FCM_FLDI))
            obj_context->interlaced_stream = 1;

        break;
    }
    default:
        break;
    }

    /* record just what type of picture we are */
    if (obj_surface->share_info) {
        psb_surface_share_info_p share_info = obj_surface->share_info;
        if (obj_context->interlaced_stream) {
            psb__information_message("Intelaced stream, no MSVDX rotate\n");
            SET_SURFACE_INFO_rotate(obj_surface->psb_surface, 0);
            obj_context->msvdx_rotate = 0;
            share_info->bob_deinterlace = 1;
        } else {
            share_info->bob_deinterlace = 0;
        }
    }
}

/*
 * Detach a surface from obj_surface
 */
VAStatus psb_DestroyRotateSurface(
    VADriverContextP ctx,
    object_surface_p obj_surface,
    int rotate
)
{
    INIT_DRIVER_DATA;
    psb_surface_p psb_surface = obj_surface->psb_surface_rotate;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    /* Allocate alternative output surface */
    if (psb_surface) {
        psb__information_message("Try to allocate surface for alternative rotate output\n");
        psb_surface_destroy(obj_surface->psb_surface_rotate);
        free(psb_surface);

        obj_surface->psb_surface_rotate = NULL;
        obj_surface->width_r = obj_surface->width;
        obj_surface->height_r = obj_surface->height;
    }

    return vaStatus;
}

/*
 * Create and attach a rotate surface to obj_surface
 */
VAStatus psb_CreateRotateSurface(
    VADriverContextP ctx,
    object_surface_p obj_surface,
    int msvdx_rotate
)
{
    int width, height;
    psb_surface_p psb_surface;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int need_realloc = 0, protected = 0;
    psb_surface_share_info_p share_info = obj_surface->share_info;
    INIT_DRIVER_DATA;

    if (msvdx_rotate == 0
#ifdef OVERLAY_ENABLE_MIRROR
        /*Bypass 180 degree rotate when overlay enabling mirror*/
        || msvdx_rotate == VA_ROTATION_180
#endif
        )
        return vaStatus;

    psb_surface = obj_surface->psb_surface_rotate;
    if (psb_surface) {
        CHECK_SURFACE_REALLOC(psb_surface, msvdx_rotate, need_realloc);
        if (need_realloc == 0) {
            goto exit;
        } else { /* free the old rotate surface */
            /*FIX ME: No sync mechanism to hold surface buffer b/w msvdx and display(overlay).
              So Disable dynamic surface destroy/create for avoiding buffer corruption.*/
            return VA_STATUS_SUCCESS;
        }
    } else
        psb_surface = (psb_surface_p) calloc(1, sizeof(struct psb_surface_s));

    psb__information_message("Try to allocate surface for alternative rotate output\n");

    width = obj_surface->width;
    height = obj_surface->height;

    if (msvdx_rotate == 2 /* VA_ROTATION_180 */) {
        vaStatus = psb_surface_create(driver_data, width, height, VA_FOURCC_NV12,
                                      protected, psb_surface);
        obj_surface->width_r = width;
        obj_surface->height_r = height;
    } else {
        vaStatus = psb_surface_create(driver_data, obj_surface->height_origin, ((width + 0x1f) & ~0x1f), VA_FOURCC_NV12,
                                      protected, psb_surface);
        obj_surface->width_r = obj_surface->height_origin;
        obj_surface->height_r = ((width + 0x1f) & ~0x1f);
    }

    if (VA_STATUS_SUCCESS != vaStatus) {
        free(psb_surface);
        obj_surface->psb_surface_rotate = NULL;
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE;
        return vaStatus;
    }

    obj_surface->psb_surface_rotate = psb_surface;
exit:
    SET_SURFACE_INFO_rotate(psb_surface, msvdx_rotate);
    /* derive the protected flag from the primay surface */
    SET_SURFACE_INFO_protect(psb_surface,
                             GET_SURFACE_INFO_protect(obj_surface->psb_surface));

    /*notify hwc that rotated buffer is ready to use.
    * TODO: Do these in psb_SyncSurface()
    */
    if (share_info != NULL) {
        share_info->width_r = psb_surface->stride;
        share_info->height_r = obj_surface->height_r;
        share_info->rotate_khandle =
            (uint32_t)(wsbmKBufHandle(wsbmKBuf(psb_surface->buf.drm_buf)));
        share_info->metadata_rotate = VAROTATION2HAL(driver_data->va_rotate);
        share_info->surface_rotate = VAROTATION2HAL(msvdx_rotate);
    }

    return vaStatus;
}


