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
#include <va/va_drmcommon.h>
#include "psb_drv_video.h"
#include "psb_output.h"
#include "android/psb_android_glue.h"
#include "psb_drv_debug.h"
#include "vc1_defs.h"
#include "pnw_rotate.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <wsbm/wsbm_pool.h>
#include <wsbm/wsbm_manager.h>
#include <wsbm/wsbm_util.h>
#include <wsbm/wsbm_fencemgr.h>

#ifdef ANROID
#include <system/graphics.h>
#endif

#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData
#define INIT_OUTPUT_PRIV    unsigned char* output = ((psb_driver_data_p)ctx->pDriverData)->ws_priv
#define SURFACE(id)    ((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))
#define CONTEXT(id) ((object_context_p) object_heap_lookup( &driver_data->context_heap, id ))

#define CHECK_SURFACE_REALLOC(psb_surface, msvdx_rotate, need)  \
do {                                                            \
    int old_rotate = GET_SURFACE_INFO_rotate(psb_surface);      \
    switch (msvdx_rotate) {                                     \
    case 2: /* 180 */                                           \
        if (old_rotate == 2)                                    \
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

#ifdef PSBVIDEO_MRFL_VPP
#define VPP_STATUS_STORAGE "/data/data/com.intel.vpp/shared_prefs/vpp_settings.xml"
static int isVppOn() {
    FILE *handle = fopen(VPP_STATUS_STORAGE, "r");
    if(handle == NULL)
        return 0;

    const int MAXLEN = 1024;
    char buf[MAXLEN];
    memset(buf, 0 ,MAXLEN);
    if(fread(buf, 1, MAXLEN, handle) <= 0) {
        fclose(handle);
        return 0;
    }
    buf[MAXLEN - 1] = '\0';

    if(strstr(buf, "true") == NULL) {
        fclose(handle);
        return 0;
    }

    fclose(handle);
    return 1;
}
#endif

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
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "MSVDX: disable MSVDX rotation\n");
        driver_data->disable_msvdx_rotate = 1;
    }
    /* FIXME: Disable rotation when VPP enabled, just a workround here*/
#ifdef PSBVIDEO_MRFL_VPP
    if (isVppOn()) {
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "For VPP: disable MSVDX rotation\n");
        driver_data->disable_msvdx_rotate = 1;
    }
#endif
    driver_data->disable_msvdx_rotate_backup = driver_data->disable_msvdx_rotate;
}

void psb_RecalcRotate(VADriverContextP ctx, object_context_p obj_context)
{
    INIT_DRIVER_DATA;
    INIT_OUTPUT_PRIV;
    int angle, new_rotate, i;
    int old_rotate = driver_data->msvdx_rotate_want;
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    int mode = psb_android_is_extvideo_mode((void*)output);
#else
    int mode = 0;
#endif

    if (mode) {
        if (driver_data->mipi0_rotation != 0) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "Clear display rotate for extended video mode or meta data rotate");
            driver_data->mipi0_rotation = 0;
            driver_data->hdmi_rotation = 0;
        }
        if (mode == 2) {
            if( driver_data->va_rotate == 0) {
                driver_data->disable_msvdx_rotate = 1;
            } else {
                driver_data->mipi0_rotation = 0;
                driver_data->hdmi_rotation = 0;
            }
        }
        else
            driver_data->disable_msvdx_rotate = driver_data->disable_msvdx_rotate_backup;
    } else if (driver_data->native_window) {
        int display_rotate = 0;
        psb_android_surfaceflinger_rotate(driver_data->native_window, &display_rotate);
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "NativeWindow(0x%x), get surface flinger rotate %d\n", driver_data->native_window, display_rotate);

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
            if (obj_surface && obj_surface->share_info) {
                obj_surface->share_info->surface_rotate = 0;
                obj_surface->share_info->metadata_rotate = VAROTATION2HAL(driver_data->va_rotate);
            }
        }

        drv_debug_msg(VIDEO_DEBUG_GENERAL, "MSVDX: new rotation %d desired\n", new_rotate);
        driver_data->msvdx_rotate_want = new_rotate;
    }

}


void psb_CheckInterlaceRotate(object_context_p obj_context, unsigned char *pic_param_tmp)
{
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

    if (obj_surface->share_info) {
        psb_surface_share_info_p share_info = obj_surface->share_info;
        if (obj_context->interlaced_stream) {
            SET_SURFACE_INFO_rotate(obj_surface->psb_surface, 0);
            obj_context->msvdx_rotate = 0;
            share_info->bob_deinterlace = 1;
        } else {
           share_info->bob_deinterlace = 0;
       }
    }
}
#if 0
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
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "Try to allocate surface for alternative rotate output\n");
        psb_surface_destroy(obj_surface->psb_surface_rotate);
        free(psb_surface);

        obj_surface->psb_surface_rotate = NULL;
        obj_surface->width_r = obj_surface->width;
        obj_surface->height_r = obj_surface->height;
    }

    return vaStatus;
}
#endif
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
    int need_realloc = 0;
    unsigned int flags = 0;
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
            /*FIX ME: it is not safe to do that because surfaces may be in use for rendering.*/
            psb_surface_destroy(obj_surface->psb_surface_rotate);
            memset(psb_surface, 0, sizeof(*psb_surface));
        }
    } else {
        psb_surface = (psb_surface_p) calloc(1, sizeof(struct psb_surface_s));
        CHECK_ALLOCATION(psb_surface);
    }

#ifdef PSBVIDEO_MSVDX_DEC_TILING
    SET_SURFACE_INFO_tiling(psb_surface, GET_SURFACE_INFO_tiling(obj_surface->psb_surface));
#endif
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "Try to allocate surface for alternative rotate output\n");

    width = obj_surface->width;
    height = obj_surface->height;
    flags = IS_ROTATED;

    if (msvdx_rotate == 2 /* VA_ROTATION_180 */) {
        vaStatus = psb_surface_create(driver_data, width, height, VA_FOURCC_NV12,
                                      flags, psb_surface);
        obj_surface->width_r = width;
        obj_surface->height_r = height;
    } else {
        vaStatus = psb_surface_create(driver_data, obj_surface->height_origin, ((width + 0x1f) & ~0x1f), VA_FOURCC_NV12,
                                      flags, psb_surface);
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

#ifdef PSBVIDEO_MSVDX_DEC_TILING
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "attempt to update tile context\n");
    if (GET_SURFACE_INFO_tiling(psb_surface)) {
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "update tile context\n");
        object_context_p obj_context = CONTEXT(obj_surface->context_id);
        if (NULL == obj_context) {
            vaStatus = VA_STATUS_ERROR_INVALID_CONTEXT;
            DEBUG_FAILURE;
            return vaStatus;
        }
        unsigned long msvdx_tile = psb__tile_stride_log2_256(obj_surface->width_r);
        obj_context->msvdx_tile &= 0xf; /* clear rotate tile */
        obj_context->msvdx_tile |= (msvdx_tile << 4);
        obj_context->ctp_type &= (~PSB_CTX_TILING_MASK); /* clear tile context */
        obj_context->ctp_type |= ((obj_context->msvdx_tile & 0xff) << 16);
        psb_update_context(driver_data, obj_context->ctp_type);
    }
#endif

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

        share_info->rotate_luma_stride = psb_surface->stride;
        share_info->rotate_chroma_u_stride = psb_surface->stride;
        share_info->rotate_chroma_v_stride = psb_surface->stride;
    }

    return vaStatus;
}


