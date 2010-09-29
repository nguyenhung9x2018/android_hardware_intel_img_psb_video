/*
 * INTEL CONFIDENTIAL
 * Copyright 2007 Intel Corporation. All Rights Reserved.
 * Copyright 2005-2007 Imagination Technologies Limited. All Rights Reserved.
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

#include <stdio.h>
#include <math.h>

#include <psb_drm.h>
#include <va/va_backend.h>
#include <va/va_dricommon.h>

#include <wsbm/wsbm_manager.h>
#include <X11/Xlib.h>
#include <X11/X.h>

#include "psb_drv_video.h"
#include "psb_x11.h"
#include "psb_output.h"
#include "psb_xrandr.h"
#include "psb_surface_ext.h"

#include "psb_texture.h"

#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;
#define INIT_OUTPUT_PRIV    psb_x11_output_p output = (psb_x11_output_p)(((psb_driver_data_p)ctx->pDriverData)->ws_priv)

#define SURFACE(id) ((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &driver_data->buffer_heap, id ))
#define IMAGE(id)  ((object_image_p) object_heap_lookup( &driver_data->image_heap, id ))
#define SUBPIC(id)  ((object_subpic_p) object_heap_lookup( &driver_data->subpic_heap, id ))
#define CONTEXT(id) ((object_context_p) object_heap_lookup( &driver_data->context_heap, id ))

static VAStatus psb_dri_init(VADriverContextP ctx, VASurfaceID surface, Drawable draw)
{
    INIT_DRIVER_DATA;
    INIT_OUTPUT_PRIV;
    int i, ret;

    union dri_buffer *dri_buffer;
    union dri_buffer *extend_dri_buffer;

    PPVR2DMEMINFO dri2_bb_export_meminfo;

    struct psb_texture_s *texture_priv = &driver_data->ctexture_priv;

    if (psb_xrandr_extvideo_mode()) {

	output->extend_drawable = (Window)psb_xrandr_create_full_screen_window();

	texture_priv->extend_dri_drawable = dri_get_drawable(ctx, output->extend_drawable);
	if (!texture_priv->extend_dri_drawable) {
	    psb__error_message("%s(): Failed to get extend_dri_drawable\n", __func__);
	    return VA_STATUS_ERROR_UNKNOWN;
	}

	extend_dri_buffer = dri_get_rendering_buffer(ctx, texture_priv->extend_dri_drawable);
	if (!extend_dri_buffer) {
	    psb__error_message("%s(): Failed to get extend_dri_buffer\n", __func__);
	    return VA_STATUS_ERROR_UNKNOWN;
	}

	ret = PVR2DMemMap(texture_priv->hPVR2DContext, 0, (PVR2D_HANDLE)extend_dri_buffer->dri2.name, &dri2_bb_export_meminfo);
	if (ret != PVR2D_OK) {
	    psb__error_message("%s(): PVR2DMemMap failed, ret = %d\n", __func__, ret);
	    return VA_STATUS_ERROR_UNKNOWN;
	}

	memcpy(&texture_priv->extend_dri2_bb_export, dri2_bb_export_meminfo->pBase, sizeof(PVRDRI2BackBuffersExport));

	//must be Flip-Chain mode
	for(i = 0; i < DRI2_BLIT_BUFFERS_NUM; i++) {
	    ret = PVR2DMemMap(texture_priv->hPVR2DContext, 0, texture_priv->extend_dri2_bb_export.hBuffers[i], &texture_priv->extend_blt_meminfo[i]);
	    if (ret != PVR2D_OK) {
		psb__error_message("%s(): PVR2DMemMap failed, ret = %d\n", ret);
		return VA_STATUS_ERROR_UNKNOWN;
	    }
	}
    }

    texture_priv->dri_drawable = dri_get_drawable(ctx, output->output_drawable);
    if (!texture_priv->dri_drawable) {
        psb__error_message("%s(): Failed to get dri_drawable\n", __func__);
	return VA_STATUS_ERROR_UNKNOWN;
    }

    dri_buffer = dri_get_rendering_buffer(ctx, texture_priv->dri_drawable);
    if (!dri_buffer) {
        psb__error_message("%s(): Failed to get dri_buffer\n", __func__);
	return VA_STATUS_ERROR_UNKNOWN;
    }

    ret = PVR2DMemMap(texture_priv->hPVR2DContext, 0, (PVR2D_HANDLE)dri_buffer->dri2.name, &dri2_bb_export_meminfo);
    if (ret != PVR2D_OK) {
	psb__error_message("%s(): PVR2DMemMap failed, ret = %d\n", __func__, ret);
	return VA_STATUS_ERROR_UNKNOWN;
    }

    memcpy(&texture_priv->dri2_bb_export, dri2_bb_export_meminfo->pBase, sizeof(PVRDRI2BackBuffersExport));

    if (texture_priv->dri2_bb_export.ui32Type == DRI2_BACK_BUFFER_EXPORT_TYPE_BUFFERS) {
	psb__information_message("psb_dri_init: Now map buffer, DRI2 back buffer export type: DRI2_BACK_BUFFER_EXPORT_TYPE_BUFFERS\n");

	for(i = 0; i < DRI2_BLIT_BUFFERS_NUM; i++) {
	    ret = PVR2DMemMap(texture_priv->hPVR2DContext, 0, texture_priv->dri2_bb_export.hBuffers[i], &texture_priv->blt_meminfo[i]);
	    if (ret != PVR2D_OK) {
		psb__error_message("%s(): PVR2DMemMap failed, ret = %d\n", ret);
		return VA_STATUS_ERROR_UNKNOWN;
	    }
	}
    } else if (texture_priv->dri2_bb_export.ui32Type == DRI2_BACK_BUFFER_EXPORT_TYPE_SWAPCHAIN) {
	psb__information_message("psb_dri_init: Now map buffer, DRI2 back buffer export type: DRI2_BACK_BUFFER_EXPORT_TYPE_SWAPCHAIN\n");

	for(i = 0; i < DRI2_FLIP_BUFFERS_NUM; i++) {
	    ret = PVR2DMemMap(texture_priv->hPVR2DContext, 0, texture_priv->dri2_bb_export.hBuffers[i], &texture_priv->flip_meminfo[i]);
	    if (ret != PVR2D_OK) {
		psb__error_message("%s(): PVR2DMemMap failed, ret = %d\n", ret);
		return VA_STATUS_ERROR_UNKNOWN;
	    }
	}
    }

    texture_priv->dri_init_flag = 1;

    PVR2DMemFree(texture_priv->hPVR2DContext, dri2_bb_export_meminfo);
    return VA_STATUS_SUCCESS;
}

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
)
{
    INIT_DRIVER_DATA;
    INIT_OUTPUT_PRIV;
    int ret;
    int primary_crtc_x, primary_crtc_y, extend_crtc_x, extend_crtc_y;
    int display_width, display_height, extend_display_width, extend_display_height;
    psb_xrandr_location extend_location;
    object_surface_p obj_surface = SURFACE(surface);
    psb_surface_p psb_surface;

    struct psb_texture_s *texture_priv = &driver_data->ctexture_priv;

    obj_surface = SURFACE(surface);
    psb_surface = obj_surface->psb_surface;

    if (output->output_drawable != draw) {
	output->output_drawable = draw;
        ret = psb_xrandr_init(ctx);
        if ( ret != 0) {
	    psb__error_message("%s: Failed to initialize psb xrandr error # %d\n", __func__, ret);
	    return VA_STATUS_ERROR_UNKNOWN;
        }
    }

    if (driver_data->use_xrandr_thread && !driver_data->xrandr_thread_id) {
        ret = psb_xrandr_thread_create(ctx);
        if ( ret != 0) {
	    psb__error_message("%s: Failed to create psb xrandr thread error # %d\n", __func__, ret);
	    return VA_STATUS_ERROR_UNKNOWN;
        }
    }

    if (!texture_priv->dri_init_flag) {
	ret = psb_dri_init(ctx, surface, draw);
	if (ret != VA_STATUS_SUCCESS)
	    return VA_STATUS_ERROR_UNKNOWN;
    }

    if (psb_xrandr_clone_mode()) {
	psb__information_message("psb_putsurface_ctexture: current mode is Clone\n");
    } else if (psb_xrandr_extend_mode()) {
	ret = psb_xrandr_primary_crtc_coordinate(&primary_crtc_x, &primary_crtc_y, &display_width, &display_height);
	if (ret != 0) {
	    psb__error_message("%s: Failed to get primary crtc coordinates error # %d\n", __func__, ret);
	    return VA_STATUS_ERROR_UNKNOWN;
	}
	display_width += 1;
	display_height += 1;

	ret = psb_xrandr_extend_crtc_coordinate(&extend_crtc_x, &extend_crtc_y, &extend_display_width, &extend_display_height, &extend_location);
	if (ret != 0) {
	    psb__error_message("%s: Failed to get extend crtc coordinates error # %d\n", __func__, ret);
	    return VA_STATUS_ERROR_UNKNOWN;
	}
	extend_display_width += 1;
	extend_display_height += 1;

	switch (extend_location) {
	    case LEFT_OF:
		display_height = display_height > extend_display_height ? display_height : extend_display_height;
		if (destw > display_width + extend_display_width)
		    destw = display_width + extend_display_width;
		if (desth > display_height)
		    desth = display_height;
	    break;
	    case RIGHT_OF:
		display_height = display_height > extend_display_height ? display_height : extend_display_height;
		if (destw > display_width + extend_display_width)
		    destw = display_width + extend_display_width;
		if (desth > display_height)
		    desth = display_height;
	    break;
	    case BELOW:
		display_width = display_width > extend_display_width ? display_width : extend_display_width;
		if (destw > display_width)
		    destw = display_width;
		if (desth > display_height + extend_display_height)
		    desth = display_height + extend_display_height;
	    break;
	    case ABOVE:
		display_width = display_width > extend_display_width ? display_width : extend_display_width;
		if (destw > display_width)
		    destw = display_width;
		if (desth > display_height + extend_display_height)
		    desth = display_height + extend_display_height;
	    break;
	    case NORMAL:
	    default:
	    break;
	}
	psb__information_message("psb_putsurface_ctexture: current mode is Extend, Location: %08x\n", extend_location);
    } else if (psb_xrandr_extvideo_mode()) {

	psb__information_message("psb_putsurface_ctexture: current mode is ExtVideo, Location: %08x\n", extend_location);
	ret = psb_xrandr_primary_crtc_coordinate(&primary_crtc_x, &primary_crtc_y, &display_width, &display_height);
	if (ret != 0) {
	    psb__error_message("%s: Failed to get primary crtc coordinates error # %d\n", __func__, ret);
	    return VA_STATUS_ERROR_UNKNOWN;
	}
	display_width += 1;
	display_height += 1;

	ret = psb_xrandr_extend_crtc_coordinate(&extend_crtc_x, &extend_crtc_y, &extend_display_width, &extend_display_height, &extend_location);
	if (ret != 0) {
	    psb__error_message("%s: Failed to get extend crtc coordinates error # %d\n", __func__, ret);
	    return VA_STATUS_ERROR_UNKNOWN;
	}
	extend_display_width += 1;
	extend_display_height += 1;

	psb__information_message("psb_putsurface_ctexture: ExtVideo coordinate: srcx= %d, srcy=%d, srcw=%d, srch=%d, destx=%d, desty=%d, destw=%d, desth=%d, cur_buffer=%d\n", 
				  srcx, srcy, srcw, srch, destx, desty, extend_display_width, extend_display_height, texture_priv->current_blt_buffer);

	psb_putsurface_textureblit(ctx, texture_priv->extend_blt_meminfo[texture_priv->extend_current_blt_buffer], surface, srcx, srcy, srcw, srch, extend_crtc_x, extend_crtc_y,
				extend_display_width, extend_display_height,
				obj_surface->width, obj_surface->height,
				psb_surface->stride, psb_surface->buf.drm_buf,
				psb_surface->buf.pl_flags);

	dri_swap_buffer(ctx, texture_priv->extend_dri_drawable);
	texture_priv->extend_current_blt_buffer = (texture_priv->extend_current_blt_buffer + 1) & 0x01;
     }

    if (texture_priv->dri2_bb_export.ui32Type == DRI2_BACK_BUFFER_EXPORT_TYPE_BUFFERS) {
	psb__information_message("psb_putsurface_ctexture: SWAP BUFFER, Video coordinate: srcx= %d, srcy=%d, srcw=%d, srch=%d, destx=%d, desty=%d, destw=%d, desth=%d, cur_buffer=%d\n", 
 				  srcx, srcy, srcw, srch, destx, desty, destw, desth, texture_priv->current_blt_buffer);
	//FIXME: Currently using Extend Mode to simulate ExtVideo mode.
	//When resolution >= 864x480, Extend video has a white line on top. 
	//When resolution >= 864x480, testsuite should create a window with maximum resolution of 864x480, but now it creates window based on the virtual frame buffer resolution.
	//When ExtVideo driver is ready, double check the above 2 issue.
	psb_putsurface_textureblit(ctx, texture_priv->blt_meminfo[texture_priv->current_blt_buffer], surface, srcx, srcy, srcw, srch, destx, desty, destw, desth,
				obj_surface->width, obj_surface->height,
				psb_surface->stride, psb_surface->buf.drm_buf,
				psb_surface->buf.pl_flags);

	dri_swap_buffer(ctx, texture_priv->dri_drawable);
	texture_priv->current_blt_buffer = (texture_priv->current_blt_buffer + 1) & 0x01;

    } else if (texture_priv->dri2_bb_export.ui32Type == DRI2_BACK_BUFFER_EXPORT_TYPE_SWAPCHAIN) {
	psb__information_message("psb_putsurface_ctexture: FLIP CHAIN, Video coordinate: srcx= %d, srcy=%d, srcw=%d, srch=%d, destx=%d, desty=%d, destw=%d, desth=%d, cur_buffer=%d\n", 
 				  srcx, srcy, srcw, srch, destx, desty, texture_priv->rootwin_width, texture_priv->rootwin_height, texture_priv->current_blt_buffer);

	psb_putsurface_textureblit(ctx, texture_priv->flip_meminfo[texture_priv->current_blt_buffer], surface, srcx, srcy, srcw, srch, destx, desty, texture_priv->rootwin_width, texture_priv->rootwin_height,
				obj_surface->width, obj_surface->height,
				psb_surface->stride, psb_surface->buf.drm_buf,
				psb_surface->buf.pl_flags);

	dri_swap_buffer(ctx, texture_priv->dri_drawable);
	texture_priv->current_blt_buffer++;
	if(texture_priv->current_blt_buffer == DRI2_FLIP_BUFFERS_NUM)
	    texture_priv->current_blt_buffer = 0;
    }

    return VA_STATUS_SUCCESS;
}
