/*
 * Copyright (c) 2011 Intel Corporation. All Rights Reserved.
 * Copyright (c) Imagination Technologies Limited, UK
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

/*
 * Authors:
 *    Li Zeng <li.zeng@intel.com>
 */

#include "tng_vld_dec.h"
#include "psb_drv_debug.h"
#include "hwdefs/dxva_fw_ctrl.h"
#include "hwdefs/reg_io2.h"
#include "hwdefs/msvdx_offsets.h"
#include "hwdefs/msvdx_cmds_io2.h"

static void tng_yuv_processor_QueryConfigAttributes(
    VAProfile profile,
    VAEntrypoint entrypoint,
    VAConfigAttrib *attrib_list,
    int num_attribs)
{
    /* No specific attributes */
}

static VAStatus tng_yuv_processor_ValidateConfig(
    object_config_p obj_config)
{
    return VA_STATUS_SUCCESS;
}

static VAStatus tng_yuv_processor_CreateContext(
    object_context_p obj_context,
    object_config_p obj_config)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_DEC_p dec_ctx = (context_DEC_p) obj_context->format_data;
    context_yuv_processor_p ctx;

    ctx = (context_yuv_processor_p) malloc(sizeof(struct context_yuv_processor_s));
    CHECK_ALLOCATION(ctx);

    dec_ctx->yuv_ctx = ctx;

    return vaStatus;
}

static void tng_yuv_processor_DestroyContext(
    object_context_p obj_context)
{
    context_DEC_p dec_ctx = (context_DEC_p) obj_context->format_data;

    if (dec_ctx->yuv_ctx) {
        free(dec_ctx->yuv_ctx);
        dec_ctx->yuv_ctx = NULL;
    }
}

static VAStatus tng_yuv_processor_BeginPicture(
    object_context_p obj_context)
{
    return VA_STATUS_SUCCESS;
}

static void tng__yuv_processor_process(context_DEC_p dec_ctx)
{
    context_yuv_processor_p ctx = dec_ctx->yuv_ctx;
    psb_cmdbuf_p cmdbuf = dec_ctx->obj_context->cmdbuf;
    psb_surface_p target_surface = dec_ctx->obj_context->current_render_target->psb_surface;
    psb_buffer_p buffer;
    uint32_t reg_value;

    psb_cmdbuf_rendec_start(cmdbuf, RENDEC_REGISTER_OFFSET(MSVDX_CMDS, DISPLAY_PICTURE_SIZE));

    reg_value = 0;
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, DISPLAY_PICTURE_SIZE, DISPLAY_PICTURE_HEIGHT, (ctx->display_height) - 1);
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, DISPLAY_PICTURE_SIZE, DISPLAY_PICTURE_WIDTH, (ctx->display_width) - 1);
    psb_cmdbuf_rendec_write(cmdbuf, reg_value);

    reg_value = 0;
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, CODED_PICTURE_SIZE, CODED_PICTURE_HEIGHT, (ctx->coded_height) - 1);
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, CODED_PICTURE_SIZE, CODED_PICTURE_WIDTH, (ctx->coded_width) - 1);
    psb_cmdbuf_rendec_write(cmdbuf, reg_value);
    psb_cmdbuf_rendec_end(cmdbuf);


    /*TODO add stride and else there*/
    reg_value = 0;
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, OPERATING_MODE, CODEC_MODE, 3);
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, OPERATING_MODE, ASYNC_MODE, 1);
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, OPERATING_MODE, CODEC_PROFILE, 1);
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, OPERATING_MODE, CHROMA_FORMAT,	1);
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, OPERATING_MODE, ROW_STRIDE, target_surface->stride_mode);

    psb_cmdbuf_rendec_start(cmdbuf, RENDEC_REGISTER_OFFSET( MSVDX_CMDS, OPERATING_MODE ));
    psb_cmdbuf_rendec_write(cmdbuf, reg_value);
    psb_cmdbuf_rendec_end(cmdbuf);

    psb_cmdbuf_rendec_start(cmdbuf, RENDEC_REGISTER_OFFSET(MSVDX_CMDS, REFERENCE_PICTURE_BASE_ADDRESSES));
    buffer = &target_surface->buf;
    psb_cmdbuf_rendec_write_address(cmdbuf, buffer, buffer->buffer_ofs);
    psb_cmdbuf_rendec_write_address(cmdbuf, buffer,
                                    buffer->buffer_ofs +
                                    target_surface->chroma_offset);
    psb_cmdbuf_rendec_end(cmdbuf);

    reg_value = 0;
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, SLICE_PARAMS, CONSTRAINED_INTRA_PRED, 0 );
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, SLICE_PARAMS, MODE_CONFIG, 0 );
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, SLICE_PARAMS, DISABLE_DEBLOCK_FILTER_IDC, 1 );
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, SLICE_PARAMS, SLICE_ALPHA_CO_OFFSET_DIV2, 0 );
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, SLICE_PARAMS, SLICE_BETA_OFFSET_DIV2, 0 );
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, SLICE_PARAMS, SLICE_FIELD_TYPE, 2 );
    REGIO_WRITE_FIELD_LITE(reg_value, MSVDX_CMDS, SLICE_PARAMS, SLICE_CODE_TYPE, 1 ); // P
    *dec_ctx->p_slice_params = reg_value;

    psb_cmdbuf_rendec_start(cmdbuf, RENDEC_REGISTER_OFFSET( MSVDX_CMDS, SLICE_PARAMS ) );
    psb_cmdbuf_rendec_write(cmdbuf, reg_value);
    psb_cmdbuf_rendec_end(cmdbuf);

    vld_dec_setup_alternative_frame(dec_ctx->obj_context);

    *cmdbuf->cmd_idx++ = CMD_DEBLOCK | CMD_DEBLOCK_TYPE_SKIP;
    *cmdbuf->cmd_idx++ = 0;
    *cmdbuf->cmd_idx++ = ctx->coded_width / 16;
    *cmdbuf->cmd_idx++ = ctx->coded_height / 16;
    *cmdbuf->cmd_idx++ = 0;
    *cmdbuf->cmd_idx++ = 0;

}

static VAStatus tng__yuv_processor_execute(context_DEC_p dec_ctx, object_buffer_p obj_buffer)
{
    psb_surface_p target_surface = dec_ctx->obj_context->current_render_target->psb_surface;
    context_yuv_processor_p ctx = dec_ctx->yuv_ctx;
    uint32_t reg_value;
    VAStatus vaStatus;

    ASSERT(obj_buffer->type == YUVProcessorSurfaceType);
    ASSERT(obj_buffer->num_elements == 1);
    ASSERT(obj_buffer->size == sizeof(struct surface_param_s));
    ASSERT(target_surface);

    if ((obj_buffer->num_elements != 1) ||
        (obj_buffer->size != sizeof(struct surface_param_s)) ||
        (NULL == target_surface)) {
        return VA_STATUS_ERROR_UNKNOWN;
    }

    surface_param_p surface_params = (surface_param_p) obj_buffer->buffer_data;
    ctx->display_width = surface_params->display_width;
    ctx->display_height = surface_params->display_height;
    ctx->coded_width = surface_params->coded_width;
    ctx->coded_height = surface_params->coded_height;
    ctx->target_surface = target_surface;
#ifdef ADNROID
    LOGE("%s, %d %d %d %d***************************************************\n",
		 __func__, ctx->display_width, ctx->display_height, ctx->coded_width, ctx->coded_height);
#endif
    vaStatus = VA_STATUS_SUCCESS;

    if (psb_context_get_next_cmdbuf(dec_ctx->obj_context)) {
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        DEBUG_FAILURE;
        return vaStatus;
    }
    /* ctx->begin_slice(ctx, slice_param); */
    vld_dec_FE_state(dec_ctx->obj_context, NULL);

    tng__yuv_processor_process(dec_ctx);
    /* ctx->process_slice(ctx, slice_param); */
    vld_dec_write_kick(dec_ctx->obj_context);

    dec_ctx->obj_context->video_op = psb_video_vld;
    dec_ctx->obj_context->flags = 0;

    /* ctx->end_slice(ctx); */
    dec_ctx->obj_context->flags = FW_VA_RENDER_IS_FIRST_SLICE | FW_VA_RENDER_IS_LAST_SLICE | FW_INTERNAL_CONTEXT_SWITCH;

    if (psb_context_submit_cmdbuf(dec_ctx->obj_context)) {
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
    }
    return vaStatus;
}

static VAStatus tng_yuv_processor_process_buffer(
    context_DEC_p dec_ctx,
    object_buffer_p buffer)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_buffer_p obj_buffer = buffer;

    {
        switch (obj_buffer->type) {
        case YUVProcessorSurfaceType:
            vaStatus = tng__yuv_processor_execute(dec_ctx, obj_buffer);
            DEBUG_FAILURE;
            break;

        default:
            vaStatus = VA_STATUS_ERROR_UNKNOWN;
            DEBUG_FAILURE;
        }
    }

    return vaStatus;
}


VAStatus tng_yuv_processor_RenderPicture(
    object_context_p obj_context,
    object_buffer_p *buffers,
    int num_buffers)
{
    int i;
    context_DEC_p ctx = (context_DEC_p) obj_context->format_data;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    for (i = 0; i < num_buffers; i++) {
        object_buffer_p obj_buffer = buffers[i];

        switch (obj_buffer->type) {

        default:
            vaStatus = tng_yuv_processor_process_buffer(ctx, obj_buffer);
            DEBUG_FAILURE;
        }
        if (vaStatus != VA_STATUS_SUCCESS) {
            break;
        }
    }

    return vaStatus;
}

static VAStatus tng_yuv_processor_EndPicture(
    object_context_p obj_context)
{
    //INIT_CONTEXT_YUV_PROCESSOR

    if (psb_context_flush_cmdbuf(obj_context)) {
        return VA_STATUS_ERROR_UNKNOWN;
    }
    return VA_STATUS_SUCCESS;
}

struct format_vtable_s tng_yuv_processor_vtable = {
queryConfigAttributes:
    tng_yuv_processor_QueryConfigAttributes,
validateConfig:
    tng_yuv_processor_ValidateConfig,
createContext:
    tng_yuv_processor_CreateContext,
destroyContext:
    tng_yuv_processor_DestroyContext,
beginPicture:
    tng_yuv_processor_BeginPicture,
renderPicture:
    tng_yuv_processor_RenderPicture,
endPicture:
    tng_yuv_processor_EndPicture
};
