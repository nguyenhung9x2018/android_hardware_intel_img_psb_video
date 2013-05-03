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
 *    Zhangfei Zhang <zhangfei.zhang@intel.com>
 *    Mingruo Sun <mingruo.sun@intel.com>
 *
 */
#include "vsp_VPP.h"
#include "vsp_vp8.h"
#include "psb_buffer.h"
#include "psb_surface.h"
#include "vsp_cmdbuf.h"
#include "psb_drv_debug.h"
#include "va/va_enc_vp8.h"

#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;
#define INIT_CONTEXT_VPP    context_VPP_p ctx = (context_VPP_p) obj_context->format_data;
#define CONFIG(id)  ((object_config_p) object_heap_lookup( &driver_data->config_heap, id ))
#define CONTEXT(id) ((object_context_p) object_heap_lookup( &driver_data->context_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &driver_data->buffer_heap, id ))

#define SURFACE(id)    ((object_surface_p) object_heap_lookup( &ctx->obj_context->driver_data->surface_heap, id ))

#define KB 1024
#define MB (KB * KB)
#define VSP_VP8ENC_STATE_SIZE (48*MB)

#define ALIGN_TO_128(value) ((value + 128 - 1) & ~(128 - 1))

#define REF_FRAME_WIDTH  1920
#define REF_FRAME_HEIGHT 1088
#define REF_FRAME_BORDER   32

#define XMEM_FRAME_BUFFER_SIZE_IN_BYTE ((REF_FRAME_WIDTH + 2 * REF_FRAME_BORDER) * (REF_FRAME_HEIGHT + 2 * REF_FRAME_BORDER) + \
        2 * ((REF_FRAME_WIDTH + 2 * REF_FRAME_BORDER) >> 1) * (REF_FRAME_HEIGHT / 2 + REF_FRAME_BORDER)) // Allocated for HD

enum filter_status {
    FILTER_DISABLED = 0,
    FILTER_ENABLED
};

#define FUNCTION_NAME \
    printf("ENTER %s.\n",__FUNCTION__);

#define EXIT_FUNCTION_NAME \
    printf("EXIT %s.\n",__FUNCTION__);


static void vsp_VP8_DestroyContext(object_context_p obj_context);

static VAStatus vsp__VP8_check_legal_picture(object_context_p obj_context, object_config_p obj_config);

static void vsp_VP8_QueryConfigAttributes(
    VAProfile profile,
    VAEntrypoint entrypoint,
    VAConfigAttrib *attrib_list,
    int num_attribs)
{
    /* No VP8 specific attributes for now*/
    return;
}

static VAStatus vsp_VP8_ValidateConfig(
    object_config_p obj_config)
{
    int i;
    /* Check all attributes */
    for (i = 0; i < obj_config->attrib_count; i++) {
        switch (obj_config->attrib_list[i].type) {
            case VAConfigAttribRTFormat:
                /* Ignore */
                break;

            default:
                return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;
        }
    }

    return VA_STATUS_SUCCESS;
}

static VAStatus vsp__VP8_check_legal_picture(object_context_p obj_context, object_config_p obj_config)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
/*
    if (NULL == obj_context) {
        vaStatus = VA_STATUS_ERROR_INVALID_CONTEXT;
        DEBUG_FAILURE;
        return vaStatus;
    }

    if (NULL == obj_config) {
        vaStatus = VA_STATUS_ERROR_INVALID_CONFIG;
        DEBUG_FAILURE;
        return vaStatus;
    }
*/
    return vaStatus;
}

static VAStatus vsp_VP8_CreateContext(
    object_context_p obj_context,
    object_config_p obj_config)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_VPP_p ctx;
    int i;

    /* Validate flag */
    /* Validate picture dimensions */
    vaStatus = vsp__VP8_check_legal_picture(obj_context, obj_config);
    if (VA_STATUS_SUCCESS != vaStatus) {
        DEBUG_FAILURE;
        return vaStatus;
    }

    ctx = (context_VPP_p) calloc(1, sizeof(struct context_VPP_s));
    if (NULL == ctx) {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE;
        return vaStatus;
    }

    ctx->filters = NULL;
    ctx->num_filters = 0;

    /* set size */
    ctx->param_sz = 0;
    ctx->pic_param_sz = ALIGN_TO_128(sizeof(struct VssVp8encPictureParameterBuffer));
    ctx->param_sz += ctx->pic_param_sz;
    ctx->seq_param_sz = ALIGN_TO_128(sizeof(struct VssVp8encSequenceParameterBuffer));
    ctx->param_sz += ctx->seq_param_sz;

    /* set offset */
    ctx->pic_param_offset = 0;
    ctx->seq_param_offset = ctx->pic_param_sz;

    ctx->context_buf = (psb_buffer_p) calloc(1, sizeof(struct psb_buffer_s));
    if (NULL == ctx->context_buf) {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE;
        goto out;
    }

    vaStatus = psb_buffer_create(obj_context->driver_data, VSP_VP8ENC_STATE_SIZE, psb_bt_vpu_only, ctx->context_buf);

    if (VA_STATUS_SUCCESS != vaStatus) {
        goto out;
    }

    obj_context->format_data = (void*) ctx;
    ctx->obj_context = obj_context;
/*
    for (i = 0; i < obj_config->attrib_count; ++i) {
        if (VAConfigAttribRTFormat == obj_config->attrib_list[i].type) {
            switch (obj_config->attrib_list[i].value) {
                case VA_RT_FORMAT_YUV420:
                    ctx->format = VSP_NV12;
                    break;
                case VA_RT_FORMAT_YUV422:
                    ctx->format = VSP_NV16;
                default:
                    ctx->format = VSP_NV12;
                    break;
            }
        break;
        }
    }
*/
    return vaStatus;

out:
    vsp_VP8_DestroyContext(obj_context);

    if (ctx)
        free(ctx);

    return vaStatus;
}

static void vsp_VP8_DestroyContext(
    object_context_p obj_context)
{
    INIT_CONTEXT_VPP;

    if (ctx->context_buf) {
        psb_buffer_destroy(ctx->context_buf);
        free(ctx->context_buf);
        ctx->context_buf = NULL;
    }

    if (ctx->filters) {
        free(ctx->filters);
        ctx->num_filters = 0;
    }

    free(obj_context->format_data);
    obj_context->format_data = NULL;
}

static VAStatus vsp_vp8_process_seqence_param(
    context_VPP_p ctx,
    object_buffer_p obj_buffer)
{

    VAStatus vaStatus = VA_STATUS_SUCCESS;
    vsp_cmdbuf_p cmdbuf = ctx->obj_context->vsp_cmdbuf;
    int i;

    VAEncSequenceParameterBufferVP8 *va_seq =
            (VAEncSequenceParameterBufferVP8 *) obj_buffer->buffer_data;
    struct VssVp8encSequenceParameterBuffer *seq =
            (struct VssVp8encSequenceParameterBuffer *)cmdbuf->seq_param_p;

    ctx->frame_width = va_seq->frame_width;
    ctx->frame_height = va_seq->frame_height;

    /*cmd structures initializations*/
    seq->frame_width       = va_seq->frame_width;
    seq->frame_height      = va_seq->frame_height;
    seq->rc_target_bitrate = va_seq->bits_per_second;
    seq->max_intra_rate    = 0;
    seq->rc_undershoot_pct = 100;
    seq->rc_overshoot_pct  = 100;
    /* FIXME: API doc says max 5000, but for current default test vector we still use 6000 */
    seq->rc_buf_sz         = 6000;
    seq->rc_buf_initial_sz = 4000;
    seq->rc_buf_optimal_sz = 5000;
    seq->rc_min_quantizer  = 4;
    seq->rc_max_quantizer  = 63;
    seq->kf_max_dist       = 30; //va_seq->kf_max_dist ;
    seq->kf_min_dist       = 0 ;
    seq->frame_rate        = va_seq->frame_rate;
    seq->error_resilient   = 0;
    seq->num_token_partitions = 2; // (log2: 2^2 = 4)
    seq->rc_end_usage         = 0;   /* CBR */
    seq->kf_mode              = 1;   /* AUTO */
    seq->cyclic_intra_refresh = 0;

    seq->concatenate_partitions = 1; //Make 0 not to concatenate partitions

    for (i = 0; i < 4; i++)
    {
        seq->ref_frame_buffers[i].surface_id = va_seq->reference_frames[i];// not used now.
    }

    for (i = 0; i < 4; i++) {
        object_surface_p ref_surf = SURFACE(va_seq->reference_frames[i]);
	if (!ref_surf)
		return VA_STATUS_ERROR_UNKNOWN;

        vsp_cmdbuf_reloc_pic_param(&(seq->ref_frame_buffers[i].base),
                                   ctx->seq_param_offset,
                                   &(ref_surf->psb_surface->buf),
                                   cmdbuf->param_mem_loc, seq);
    }

    vsp_cmdbuf_insert_command(cmdbuf, &cmdbuf->param_mem,
                              VssVp8encSetSequenceParametersCommand,
                              ctx->seq_param_offset,
                              sizeof(struct VssVp8encSequenceParameterBuffer));

    return vaStatus;
}



static VAStatus vsp_vp8_process_picture_param(
    psb_driver_data_p driver_data,
    context_VPP_p ctx,
    object_buffer_p obj_buffer,
    VASurfaceID surface_id)

{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    vsp_cmdbuf_p cmdbuf = ctx->obj_context->vsp_cmdbuf;

    VAEncPictureParameterBufferVP8 *va_pic =
            (VAProcPipelineParameterBuffer *) obj_buffer->buffer_data;
    struct VssVp8encPictureParameterBuffer *pic = cmdbuf->pic_param_p;
    struct VssVp8encSequenceParameterBuffer *seq =
           (struct VssVp8encSequenceParameterBuffer *)cmdbuf->seq_param_p;
    VACodedBufferSegment *p = &obj_buffer->codedbuf_mapinfo[0];

    //map parameters
    object_buffer_p pObj = BUFFER(va_pic->coded_buf); //tobe modified
    if (!pObj)
		return VA_STATUS_ERROR_UNKNOWN;

    object_surface_p src_surface = SURFACE(surface_id);

    pic->input_frame.surface_id = surface_id;
    pic->input_frame.irq        = 1;
    pic->input_frame.height     = ctx->frame_height;
    pic->input_frame.width      = ctx->frame_width;
    pic->input_frame.stride     = (ctx->frame_width + 31) & (~31);
    pic->input_frame.format     = 0; /* TODO: Specify NV12 = 0 */

    pic->version = 0;
    pic->pic_flags = (1<< 2) |  /* corresponds to  VP8_EFLAG_NO_REF_GF      */
                     (1<< 3) |  /* corresponds to  VP8_EFLAG_NO_REF_ARF     */
                     (1<< 12);   /* corresponds to ~VP8_EFLAG_NO_UPD_ENTROPY */
    pic->prev_frame_dropped = 0; /* Not yet used */
    pic->cpuused            = 5;
    pic->sharpness          = 0;
    pic->num_token_partitions = 2; /* 2^2 = 4 partitions */
    pic->encoded_frame_size = ((sizeof(struct VssVp8encEncodedFrame) + 2*1024*1024 - 1) + 31) & (~31); //pObj->size;
    pic->encoded_frame_base = pObj->buffer_data  ;//tobe modified

    {
        vsp_cmdbuf_reloc_pic_param(&(pic->encoded_frame_base),
                                   ctx->pic_param_offset, pObj->psb_buffer,
                                   cmdbuf->param_mem_loc, pic);
    }

    {
        object_surface_p cur_surf = SURFACE(surface_id);
	if(!cur_surf)
		return VA_STATUS_ERROR_UNKNOWN;

        vsp_cmdbuf_reloc_pic_param(&(pic->input_frame.base),
                                   ctx->pic_param_offset, &(cur_surf->psb_surface->buf),
                                   cmdbuf->param_mem_loc, pic);
    }

    //vsp_cmdbuf_insert_command(cmdbuf, &cmdbuf->param_mem,
    //                        VssVp8encEncodeFrameCommand,
    //                        ctx->pic_param_offset,
    //                        sizeof(VssVp8encPictureParameterBuffer));
    //vsp_cmdbuf_fence_pic_param(cmdbuf, wsbmKBufHandle(wsbmKBuf(cmdbuf->param_mem.drm_buf)));

    do { *cmdbuf->cmd_idx++ = 0;\
         *cmdbuf->cmd_idx++ = VssVp8encEncodeFrameCommand;\
         VSP_RELOC_CMDBUF(cmdbuf->cmd_idx++, ctx->pic_param_offset, &cmdbuf->param_mem);\
         *cmdbuf->cmd_idx++ = sizeof(struct VssVp8encPictureParameterBuffer);\
         *cmdbuf->cmd_idx++ = 0; *cmdbuf->cmd_idx++ = 0;\
         *cmdbuf->cmd_idx++ = wsbmKBufHandle(wsbmKBuf(pObj->psb_buffer->drm_buf)) ; \
         *cmdbuf->cmd_idx++ = wsbmKBufHandle(wsbmKBuf((&cmdbuf->param_mem)->drm_buf)); } while(0);

    return vaStatus;
}

static VAStatus vsp_VP8_RenderPicture(
    object_context_p obj_context,
    object_buffer_p *buffers,
    int num_buffers)
{

    int i;
    psb_driver_data_p driver_data = obj_context->driver_data;
    INIT_CONTEXT_VPP;
    VASurfaceID surface_id;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    for (i = 0; i < num_buffers; i++)
    {

        object_buffer_p obj_buffer = buffers[i];
        switch (obj_buffer->type) {
        case VAEncSequenceParameterBufferType:
            vaStatus = vsp_vp8_process_seqence_param(ctx, obj_buffer);
            break;
        case VAEncPictureParameterBufferType:
            surface_id = obj_context->current_render_surface_id;
            vaStatus = vsp_vp8_process_picture_param(driver_data,ctx, obj_buffer,surface_id);
            break;
        default:
            vaStatus = VA_STATUS_SUCCESS;//VA_STATUS_ERROR_UNKNOWN;
            DEBUG_FAILURE;
        }
        if (vaStatus != VA_STATUS_SUCCESS) {
            break;
        }
    }

    return vaStatus;
}

static VAStatus vsp_VP8_BeginPicture(
    object_context_p obj_context)
{
    int ret;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    INIT_CONTEXT_VPP;
    vsp_cmdbuf_p cmdbuf;

    /* Initialise the command buffer */
    ret = vsp_context_get_next_cmdbuf(ctx->obj_context);
    if (ret) {
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "get next cmdbuf fail\n");
            vaStatus = VA_STATUS_ERROR_UNKNOWN;
            return vaStatus;
     }

    cmdbuf = obj_context->vsp_cmdbuf;

    if (ctx->obj_context->frame_count == 0) /* first picture */
    {
        vsp_cmdbuf_insert_command(cmdbuf, ctx->context_buf, Vss_Sys_STATE_BUF_COMMAND,
                                  0, VSP_VP8ENC_STATE_SIZE);
    }

    /* map param mem */
    vaStatus = psb_buffer_map(&cmdbuf->param_mem, &cmdbuf->param_mem_p);
    if (vaStatus) {
        return vaStatus;
    }

    cmdbuf->pic_param_p = cmdbuf->param_mem_p;
    cmdbuf->seq_param_p = cmdbuf->param_mem_p + ctx->seq_param_offset;

    return VA_STATUS_SUCCESS;
}

static VAStatus vsp_VP8_EndPicture(
    object_context_p obj_context)
{
    INIT_CONTEXT_VPP;
    psb_driver_data_p driver_data = obj_context->driver_data;
    vsp_cmdbuf_p cmdbuf = obj_context->vsp_cmdbuf;

    if (cmdbuf->param_mem_p != NULL) {
        psb_buffer_unmap(&cmdbuf->param_mem);
        cmdbuf->param_mem_p = NULL;
        cmdbuf->pic_param_p = NULL;
        cmdbuf->end_param_p = NULL;
        cmdbuf->pipeline_param_p = NULL;
        cmdbuf->denoise_param_p = NULL;
        cmdbuf->enhancer_param_p = NULL;
        cmdbuf->sharpen_param_p = NULL;
        cmdbuf->frc_param_p = NULL;
     }

    ctx->obj_context->frame_count++;

    if (vsp_context_flush_cmdbuf(ctx->obj_context))
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "psb_VPP: flush deblock cmdbuf error\n");

    return VA_STATUS_SUCCESS;
}

struct format_vtable_s vsp_VP8_vtable = {
queryConfigAttributes:
    vsp_VP8_QueryConfigAttributes,
validateConfig:
    vsp_VP8_ValidateConfig,
createContext:
    vsp_VP8_CreateContext,
destroyContext:
    vsp_VP8_DestroyContext,
beginPicture:
    vsp_VP8_BeginPicture,
renderPicture:
    vsp_VP8_RenderPicture,
endPicture:
    vsp_VP8_EndPicture
};

