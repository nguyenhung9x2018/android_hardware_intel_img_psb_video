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
 *    Binglin Chen <binglin.chen@intel.com>
 *
 */

#include "vsp_VPP.h"
#include "psb_buffer.h"
#include "psb_surface.h"
#include "vsp_cmdbuf.h"
#include "psb_drv_debug.h"

#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;
#define INIT_CONTEXT_VPP    context_VPP_p ctx = (context_VPP_p) obj_context->format_data;
#define CONFIG(id)  ((object_config_p) object_heap_lookup( &driver_data->config_heap, id ))
#define CONTEXT(id) ((object_context_p) object_heap_lookup( &driver_data->context_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &driver_data->buffer_heap, id ))

#define SURFACE(id)    ((object_surface_p) object_heap_lookup( &ctx->obj_context->driver_data->surface_heap, id ))

#define KB 1024
#define MB (KB * KB)
#define VSP_PROC_CONTEXT_SIZE (1*MB)

#define VSP_FORWARD_REF_NUM 3

#define ALIGN_TO_128(value) ((value + 128 - 1) & ~(128 - 1))

static void vsp_VPP_DestroyContext(object_context_p obj_context);
static VAStatus vsp_set_pipeline(context_VPP_p ctx);
static VAStatus vsp_set_filter_param(context_VPP_p ctx);
	
static VAStatus vsp__VPP_check_legal_picture(object_context_p obj_context, object_config_p obj_config);

static void vsp_VPP_QueryConfigAttributes(
	VAProfile profile,
	VAEntrypoint entrypoint,
	VAConfigAttrib *attrib_list,
	int num_attribs)
{
	/* No VPP specific attributes */
	return;
}

static VAStatus vsp_VPP_ValidateConfig(
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

static VAStatus vsp__VPP_check_legal_picture(object_context_p obj_context, object_config_p obj_config)
{
	VAStatus vaStatus = VA_STATUS_SUCCESS;

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

	return vaStatus;
}

static VAStatus vsp_VPP_CreateContext(
	object_context_p obj_context,
	object_config_p obj_config)
{
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	context_VPP_p ctx;
	int i;

	/* Validate flag */
	/* Validate picture dimensions */
	vaStatus = vsp__VPP_check_legal_picture(obj_context, obj_config);
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

	ctx->frc_buf = NULL;

	/* set size */
	ctx->param_sz = 0;
	ctx->pic_param_sz = ALIGN_TO_128(sizeof(struct VssProcPictureParameterBuffer));
	ctx->param_sz += ctx->pic_param_sz;
	ctx->end_param_sz = ALIGN_TO_128(sizeof(struct VssProcPictureParameterBuffer));
	ctx->param_sz += ctx->end_param_sz;

	ctx->pipeline_param_sz = ALIGN_TO_128(sizeof(struct VssProcPipelineParameterBuffer));
	ctx->param_sz += ctx->pipeline_param_sz;
	ctx->denoise_param_sz = ALIGN_TO_128(sizeof(struct VssProcDenoiseParameterBuffer));
	ctx->param_sz += ctx->denoise_param_sz;
	ctx->enhancer_param_sz = ALIGN_TO_128(sizeof(struct VssProcColorEnhancementParameterBuffer));
	ctx->param_sz += ctx->enhancer_param_sz;
	ctx->sharpen_param_sz = ALIGN_TO_128(sizeof(struct VssProcSharpenParameterBuffer));
	ctx->param_sz += ctx->sharpen_param_sz;
	ctx->frc_param_sz = ALIGN_TO_128(sizeof(struct VssProcFrcParameterBuffer));
	ctx->param_sz += ctx->frc_param_sz;

	/* set offset */
	ctx->pic_param_offset = 0;
	ctx->end_param_offset = ctx->pic_param_offset + ctx->pic_param_sz;
	ctx->pipeline_param_offset = ctx->end_param_offset + ctx->end_param_sz;
	ctx->denoise_param_offset = ctx->pipeline_param_offset + ctx->pipeline_param_sz;
	ctx->enhancer_param_offset = ctx->denoise_param_offset + ctx->denoise_param_sz;
	ctx->sharpen_param_offset = ctx->enhancer_param_offset + ctx->enhancer_param_sz;
	ctx->frc_param_offset = ctx->sharpen_param_offset + ctx->sharpen_param_sz;

#if 0
	ctx->context_buf = (psb_buffer_p) calloc(1, sizeof(struct psb_buffer_s));
	if (NULL == ctx->context_buf) {
		vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
		DEBUG_FAILURE;
		goto out;
	}

	vaStatus = psb_buffer_create(obj_context->driver_data, VSP_PROC_CONTEXT_SIZE, psb_bt_vpu_only, ctx->context_buf);
	if (VA_STATUS_SUCCESS != vaStatus) {
		goto out;
	}
#endif
	obj_context->format_data = (void*) ctx;
	ctx->obj_context = obj_context;

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

	return vaStatus;
out:
	vsp_VPP_DestroyContext(obj_context);

	return vaStatus;
}

static void vsp_VPP_DestroyContext(
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

static VAStatus vsp__VPP_process_pipeline_param(context_VPP_p ctx, object_buffer_p obj_buffer)
{
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	vsp_cmdbuf_p cmdbuf = ctx->obj_context->vsp_cmdbuf;
	unsigned int i = 0;
	VAProcPipelineParameterBuffer *pipeline_param = (VAProcPipelineParameterBuffer *) obj_buffer->buffer_data;
	struct VssProcPictureParameterBuffer *cell_proc_picture_param = (struct VssProcPictureParameterBuffer *)cmdbuf->pic_param_p;
	struct VssProcPictureParameterBuffer *cell_end_param = (struct VssProcPictureParameterBuffer *)cmdbuf->end_param_p;
	VAProcFilterParameterBufferFrameRateConversion *frc_param;
	object_surface_p input_surface = NULL;
	object_surface_p cur_output_surf = NULL;

	/* FIXME: ignore output input color standard */

	if (pipeline_param->surface_region != NULL) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "Cann't scale\n");
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto out;
	}
		
	if (pipeline_param->output_region != NULL) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "Cann't scale\n");
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto out;
	}
	
	/* FIXME: ignore output_background_color setting  */

	if (pipeline_param->filters == NULL) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "invalid filter setting filters = %p\n", pipeline_param->filters);
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto out;
	}

#if 0
	/* for pass filter */
	if (pipeline_param->num_filters == 0 || pipeline_param->num_filters > VssProcPipelineMaxNumFilters) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "invalid filter number = %d\n", pipeline_param->num_filters);
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto out;
	}
#endif

	if (pipeline_param->forward_references == NULL) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "invalid forward_refereces %p setting\n", pipeline_param->forward_references);
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto out;
	}

	/* should we check it? since the begining it's not VSP_FORWARD_REF_NUM */
	if (pipeline_param->num_forward_references != VSP_FORWARD_REF_NUM) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "invalid num_forward_refereces %d setting, should be %d\n", pipeline_param->num_forward_references, VSP_FORWARD_REF_NUM);
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto out;
	}

	/* FIXME: no backward reference checking */
	if (!(pipeline_param->pipeline_flags & VA_PIPELINE_FLAG_END)) {
		input_surface = SURFACE(pipeline_param->surface);
		if (input_surface == NULL) {
			drv_debug_msg(VIDEO_DEBUG_ERROR, "invalid input surface %x\n", pipeline_param->surface);
			vaStatus = VA_STATUS_ERROR_UNKNOWN;
			goto out;
		}
	} else {
		input_surface = NULL;
	}
		
	/* IGNORE backward references setting */

	/* if it is the first pipeline command */
	if (pipeline_param->num_filters != ctx->num_filters || pipeline_param->num_filters == 0) {
		if (ctx->num_filters != 0) {
			drv_debug_msg(VIDEO_DEBUG_ERROR, "can not reset pipeline in the mid of post-processing or without create a new context\n");
			vaStatus = VA_STATUS_ERROR_UNKNOWN;
			goto out;
		} else {
			/* save filters */
			ctx->num_filters = pipeline_param->num_filters;
			if (ctx->num_filters == 0) {
				ctx->filters = NULL;
			} else {
				ctx->filters = (VABufferID *) calloc(ctx->num_filters, sizeof(*ctx->filters));
				if (ctx->filters == NULL) {
					drv_debug_msg(VIDEO_DEBUG_ERROR, "can not reset pipeline in the mid of post-processing or without create a new context\n");
					vaStatus = VA_STATUS_ERROR_UNKNOWN;
					goto out;
				}
				memcpy(ctx->filters, pipeline_param->filters, ctx->num_filters * sizeof(*ctx->filters));
			}

			/* set pipeline command to FW */
			vaStatus = vsp_set_pipeline(ctx);
			if (vaStatus) {
				drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to set pipeline\n");
				goto out;
			}

			/* set filter parameter to FW, record frc parameter buffer */
			vaStatus = vsp_set_filter_param(ctx);
			if (vaStatus) {
				drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to set filter parameter\n");
				goto out;
			}
		}
	} else {
		/* else ignore pipeline/filter setting  */
#if 0
		/* FIXME: we can save these check for PnP */
		for (i = 0; i < pipeline_param->num_filters; i++) {
			if (pipeline_param->filters[i] != ctx->filters[i]) {
				drv_debug_msg(VIDEO_DEBUG_ERROR, "can not reset pipeline in the mid of post-processing or without create a new context\n");
				vaStatus = VA_STATUS_ERROR_UNKNOWN;
				goto out;
			}
		}
#endif
	}

	/* fill picture command to FW */
	if (ctx->frc_buf != NULL)
		frc_param = (VAProcFilterParameterBufferFrameRateConversion *)ctx->frc_buf->buffer_data;
	else
		frc_param = NULL;

	/* end picture command */
	/* FIXME: how to acknowledge driver to send end command */
	if (pipeline_param->pipeline_flags & VA_PIPELINE_FLAG_END) {
		cell_end_param->num_input_pictures = 0;
		cell_end_param->num_output_pictures = 0;
		vsp_cmdbuf_insert_command(cmdbuf, &cmdbuf->param_mem, VssProcPictureCommand,
					  ctx->end_param_offset, sizeof(struct VssProcPictureParameterBuffer));
		goto out;
	}

	cell_proc_picture_param->num_input_pictures  = 1;
	cell_proc_picture_param->input_picture[0].surface_id = pipeline_param->surface;
	vsp_cmdbuf_reloc_pic_param(&(cell_proc_picture_param->input_picture[0].base), ctx->pic_param_offset, &(input_surface->psb_surface->buf),
				   cmdbuf->param_mem_loc, cell_proc_picture_param);
	cell_proc_picture_param->input_picture[0].height = input_surface->height_origin;
	cell_proc_picture_param->input_picture[0].width = input_surface->width;
	cell_proc_picture_param->input_picture[0].irq = 0;
//	cell_proc_picture_param->input_picture[0].stride = input_surface->psb_surface->stride;
	cell_proc_picture_param->input_picture[0].stride = 0;

	if (frc_param == NULL)
		cell_proc_picture_param->num_output_pictures = 1;
	else
		cell_proc_picture_param->num_output_pictures = frc_param->num_output_frames + 1;
	for (i = 0; i < cell_proc_picture_param->num_output_pictures; ++i) {
		if (i == 0) {
			cur_output_surf = ctx->obj_context->current_render_target;
		} else {
			cur_output_surf = SURFACE(frc_param->output_frames[i-1]);
			if (cur_output_surf == NULL) {
				drv_debug_msg(VIDEO_DEBUG_ERROR, "invalid input surface %x\n", frc_param->output_frames[i-1]);
				vaStatus = VA_STATUS_ERROR_UNKNOWN;
				goto out;
			}
		}

		cell_proc_picture_param->output_picture[i].surface_id = wsbmKBufHandle(wsbmKBuf(cur_output_surf->psb_surface->buf.drm_buf));

		vsp_cmdbuf_reloc_pic_param(&(cell_proc_picture_param->output_picture[i].base),
					   ctx->pic_param_offset, &(cur_output_surf->psb_surface->buf),
					   cmdbuf->param_mem_loc, cell_proc_picture_param);
		cell_proc_picture_param->output_picture[i].height = cur_output_surf->height_origin;
		cell_proc_picture_param->output_picture[i].width = cur_output_surf->width;
//		cell_proc_picture_param->output_picture[i].stride = cur_output_surf->psb_surface->stride;
		cell_proc_picture_param->output_picture[i].stride = 0;
		cell_proc_picture_param->output_picture[i].irq = 1;
		/* keep the same first, modify to dest format when feature's avaliable */
		cell_proc_picture_param->output_picture[i].format = ctx->format;
	}

	vsp_cmdbuf_insert_command(cmdbuf, &cmdbuf->param_mem, VssProcPictureCommand,
				  ctx->pic_param_offset, sizeof(struct VssProcPictureParameterBuffer));


	vsp_cmdbuf_fence_pic_param(cmdbuf, wsbmKBufHandle(wsbmKBuf(cmdbuf->param_mem.drm_buf)));

	/* handle reference frames, ignore backward reference */
	for (i = 0; i < pipeline_param->num_forward_references; ++i) {
		cur_output_surf = SURFACE(pipeline_param->forward_references[i]);
		if (cur_output_surf == NULL)
			continue;
		vsp_cmdbuf_buffer_ref(cmdbuf, &cur_output_surf->psb_surface->buf);
	}

out:
	free(pipeline_param);
	obj_buffer->buffer_data = NULL;
	obj_buffer->size = 0;

	return vaStatus;
}

static VAStatus vsp_VPP_RenderPicture(
	object_context_p obj_context,
	object_buffer_p *buffers,
	int num_buffers)
{
	int i;
	INIT_CONTEXT_VPP;
	VAStatus vaStatus = VA_STATUS_SUCCESS;

	for (i = 0; i < num_buffers; i++) {
		object_buffer_p obj_buffer = buffers[i];

		switch (obj_buffer->type) {
		case VAProcPipelineParameterBufferType:
			vaStatus = vsp__VPP_process_pipeline_param(ctx, obj_buffer);
			DEBUG_FAILURE;
			break;
		default:
			vaStatus = VA_STATUS_ERROR_UNKNOWN;
			DEBUG_FAILURE;
		}
		if (vaStatus != VA_STATUS_SUCCESS) {
			break;
		}
	}

	return vaStatus;
}

static VAStatus vsp_VPP_BeginPicture(
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

	/* map param mem */
	vaStatus = psb_buffer_map(&cmdbuf->param_mem, &cmdbuf->param_mem_p);
	if (vaStatus) {
		return vaStatus;
	}

	cmdbuf->pic_param_p = cmdbuf->param_mem_p + ctx->pic_param_offset;
	cmdbuf->end_param_p = cmdbuf->param_mem_p + ctx->end_param_offset;
	cmdbuf->pipeline_param_p = cmdbuf->param_mem_p + ctx->pipeline_param_offset;
	cmdbuf->denoise_param_p = cmdbuf->param_mem_p + ctx->denoise_param_offset;
	cmdbuf->enhancer_param_p = cmdbuf->param_mem_p + ctx->enhancer_param_offset;
	cmdbuf->sharpen_param_p = cmdbuf->param_mem_p + ctx->sharpen_param_offset;
	cmdbuf->frc_param_p = cmdbuf->param_mem_p + ctx->frc_param_offset;

#if 0
	if (ctx->obj_context->frame_count == 0) { /* first picture */
		psb_driver_data_p driver_data = ctx->obj_context->driver_data;

		vsp_cmdbuf_insert_command(cmdbuf, VSP_NEW_CONTEXT, 
					  wsbmBOOffsetHint(ctx->context_buf->drm_buf),
					  VSP_PROC_CONTEXT_SIZE);
	}
#endif

	return VA_STATUS_SUCCESS;
}

static VAStatus vsp_VPP_EndPicture(
	object_context_p obj_context)
{
	INIT_CONTEXT_VPP;
	psb_driver_data_p driver_data = obj_context->driver_data;
	vsp_cmdbuf_p cmdbuf = obj_context->vsp_cmdbuf;

	if(cmdbuf->param_mem_p != NULL) {
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

	if (vsp_context_flush_cmdbuf(ctx->obj_context))
		drv_debug_msg(VIDEO_DEBUG_GENERAL, "psb_VPP: flush deblock cmdbuf error\n");

	return VA_STATUS_SUCCESS;
}

struct format_vtable_s vsp_VPP_vtable = {
queryConfigAttributes:
vsp_VPP_QueryConfigAttributes,
validateConfig:
vsp_VPP_ValidateConfig,
createContext:
vsp_VPP_CreateContext,
destroyContext:
vsp_VPP_DestroyContext,
beginPicture:
vsp_VPP_BeginPicture,
renderPicture:
vsp_VPP_RenderPicture,
endPicture:
vsp_VPP_EndPicture
};

VAStatus vsp_QueryVideoProcFilters(
        VADriverContextP    ctx,
        VAContextID         context,
        VAProcFilterType   *filters,
        unsigned int       *num_filters
	)
{
	INIT_DRIVER_DATA;
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	object_context_p obj_context;
	object_config_p obj_config;
	VAEntrypoint tmp;
	int count;

	/* check if ctx is right */
	obj_context = CONTEXT(context);
	if (NULL == obj_context) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "Failed to find context\n");
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}

	obj_config = CONFIG(obj_context->config_id);
	if (NULL == obj_config) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "Failed to find config\n");
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}

	tmp = obj_config->entrypoint;
	if (tmp != VAEntrypointVideoProc) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "current entrypoint is %d, not VAEntrypointVideoProc\n", tmp);
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}

	/* check if filters is valid */
	/* check if num_filters is valid */
	if (NULL == num_filters || NULL == filters) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "invalide input parameter num_filters %p, filters %p\n", num_filters, filters);
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}

	/* check if current HW support Video proc */
	if (IS_MRFL(driver_data)) {
		count = 0;
		filters[count++] = VAProcFilterNoiseReduction;
		filters[count++] = VAProcFilterSharpening;
		filters[count++] = VAProcFilterColorEnhancement;
		filters[count++] = VAProcFilterFrameRateConversion;
		*num_filters = count;
	} else {
		*num_filters = 0;
	}
err:
	return vaStatus;
}

VAStatus vsp_QueryVideoProcFilterCaps(
        VADriverContextP    ctx,
        VAContextID         context,
        VAProcFilterType    type,
        void               *filter_caps,
        unsigned int       *num_filter_caps
	)
{
	INIT_DRIVER_DATA;
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	object_context_p obj_context;
	object_config_p obj_config;
	VAEntrypoint tmp;
	VAProcFilterCap *denoise_cap, *deblock_cap;
	VAProcFilterCap *sharpen_cap;
	VAProcFilterCap *color_enhancement_cap;
	VAProcFilterCap *frc_cap;

	/* check if context is right */
	obj_context = CONTEXT(context);
	if (NULL == obj_context) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "Failed to find context\n");
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}

	obj_config = CONFIG(obj_context->config_id);
	if (NULL == obj_config) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "Failed to find config\n");
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}

	/* check if filter_caps and num_filter_caps is right */
	if (NULL == num_filter_caps || NULL == filter_caps){
		drv_debug_msg(VIDEO_DEBUG_ERROR, "invalide input parameter num_filters %p, filters %p\n", num_filter_caps, filter_caps);
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}

	if (*num_filter_caps < 1) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "invalide input parameter num_filters == %d (> 1)\n", *num_filter_caps);
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}

	/* check if curent HW support and return corresponding caps */
	if (IS_MRFL(driver_data)) {
		/* FIXME: we should use a constant table to return caps */
		switch (type) {
		case VAProcFilterNoiseReduction:
			denoise_cap = filter_caps;
			denoise_cap->range.min_value = 0;
			denoise_cap->range.max_value = 100;
			denoise_cap->range.default_value = 50;
			denoise_cap->range.step = 1;
			break;
		case VAProcFilterDeblocking:
			deblock_cap = filter_caps;
			deblock_cap->range.min_value = 0;
			deblock_cap->range.max_value = 100;
			deblock_cap->range.default_value = 50;
			deblock_cap->range.step = 1;
			break;

		case VAProcFilterSharpening:
			sharpen_cap = filter_caps;
			sharpen_cap->range.min_value = 0;
			sharpen_cap->range.max_value = 100;
			sharpen_cap->range.default_value = 50;
			sharpen_cap->range.step = 1;
			break;

		case VAProcFilterColorEnhancement:
			color_enhancement_cap = filter_caps;
			color_enhancement_cap->range.min_value = 0;
			color_enhancement_cap->range.max_value = 100;
			color_enhancement_cap->range.default_value = 50;
			color_enhancement_cap->range.step = 1;
			break;

		case VAProcFilterFrameRateConversion:
			frc_cap = filter_caps;
			frc_cap->range.min_value = 2;
			frc_cap->range.max_value = 4;
			frc_cap->range.default_value = 2;
			/* FIXME: it's a set, step is helpless */
			frc_cap->range.step = 0.5;
			break;

		default:
			drv_debug_msg(VIDEO_DEBUG_ERROR, "invalide filter type %d\n", type);
			vaStatus = VA_STATUS_ERROR_UNKNOWN;
			*num_filter_caps = 0;
			goto err;
		}
		*num_filter_caps = 1;
	} else {
		*num_filter_caps = 0;
	}

err:
	return vaStatus;
}

VAStatus vsp_QueryVideoProcPipelineCaps(
	VADriverContextP    ctx,
        VAContextID         context,
        VABufferID         *filters,
        unsigned int        num_filters,
        VAProcPipelineCaps *pipeline_caps
    )
{
	INIT_DRIVER_DATA;
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	object_context_p obj_context;
	object_config_p obj_config;
	VAEntrypoint tmp;
	int i;

	/* check if ctx is right */
	obj_context = CONTEXT(context);
	if (NULL == obj_context) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "Failed to find context\n");
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}

	obj_config = CONFIG(obj_context->config_id);
	if (NULL == obj_config) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "Failed to find config\n");
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}

	/* check if filters and num_filters and pipeline-caps are right */
#if 0
	/* there's pass filter */
	if (num_filters == 0) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "invalid num_filters %d\n", num_filters);
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}
#endif

	if (NULL == filters || pipeline_caps == NULL) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "invalid filters %p or pipeline_caps %p\n", filters, pipeline_caps);
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}

	/* base on HW capability check the filters and return pipeline caps */
	if (IS_MRFL(driver_data)) {
		pipeline_caps->flags = 0;
		pipeline_caps->pipeline_flags = 0;
		pipeline_caps->filter_flags = 0;
		pipeline_caps->num_forward_references = VSP_FORWARD_REF_NUM;
		pipeline_caps->num_backward_references = 0;

		if (pipeline_caps->num_input_color_standards < 1) {
			drv_debug_msg(VIDEO_DEBUG_ERROR, "invalid num_input_color_standards %d\n", pipeline_caps->num_input_color_standards);
			vaStatus = VA_STATUS_ERROR_UNKNOWN;
			goto err;
		}

		/* not supported yet */
		pipeline_caps->input_color_standards[0] = VAProcColorStandardNone;
		pipeline_caps->num_input_color_standards = 1;

		if (pipeline_caps->num_output_color_standards < 1) {
			drv_debug_msg(VIDEO_DEBUG_ERROR, "invalid num_output_color_standards %d\n", pipeline_caps->num_output_color_standards);
			vaStatus = VA_STATUS_ERROR_UNKNOWN;
			goto err;
		}
			
		pipeline_caps->output_color_standards[0] = VAProcColorStandardNone;
		pipeline_caps->num_output_color_standards = 1;

		/* FIXME: should check filter value settings here */
#if 0
		for (i = 0; i < num_filters; ++i) {
			/* find buffer */
			/* map buffer */
			/* check filter buffer setting */
			/* unmap buffer */
		}
#endif
	} else {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "no HW support\n");
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto err;
	}
err:
	return vaStatus;
}

static VAStatus vsp_set_pipeline(context_VPP_p ctx)
{
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	vsp_cmdbuf_p cmdbuf = ctx->obj_context->vsp_cmdbuf;
	struct VssProcPipelineParameterBuffer *cell_pipeline_param = (struct VssProcPipelineParameterBuffer *)cmdbuf->pipeline_param_p;
	unsigned int i, j, filter_count, check_filter = 0;
	VAProcFilterParameterBufferBase *cur_param;
	enum VssProcFilterType tmp;
	psb_driver_data_p driver_data = ctx->obj_context->driver_data;

	/* init pipeline cmd */
	for (i = 0; i < VssProcPipelineMaxNumFilters; ++i)
		cell_pipeline_param->filter_pipeline[i] = -1;
	cell_pipeline_param->num_filters = 0;

	filter_count = 0;

	/* store filter buffer object */
	if (ctx->num_filters != 0) {
		for (i = 0; i < ctx->num_filters; ++i)
			ctx->filter_buf[i] = BUFFER(ctx->filters[i]);
	} else {
		goto finished;
	}

	/* loop the filter, set correct pipeline param for FW */
	for (i = 0; i < ctx->num_filters; ++i) {
		cur_param = (VAProcFilterParameterBufferBase *)ctx->filter_buf[i]->buffer_data;
		switch (cur_param->type) {
		case VAProcFilterNone: 
			goto finished;
			break;
		case VAProcFilterNoiseReduction:
		case VAProcFilterDeblocking:
			cell_pipeline_param->filter_pipeline[filter_count++] = VssProcFilterDenoise;
			check_filter++;
			break;
		case VAProcFilterSharpening:
			cell_pipeline_param->filter_pipeline[filter_count++] = VssProcFilterSharpening;
			break;
		case VAProcFilterColorEnhancement:
			cell_pipeline_param->filter_pipeline[filter_count++] = VssProcFilterColorEnhancement;
			break;
		case VAProcFilterFrameRateConversion:
			cell_pipeline_param->filter_pipeline[filter_count++] = VssProcFilterFrameRateConversion;
			break;
		default: 
			cell_pipeline_param->filter_pipeline[filter_count++] = -1;
			vaStatus = VA_STATUS_ERROR_UNKNOWN;
			goto out;
		}
	}

	/* Denoise and Deblock is alternative */
	if (check_filter >= 2) {
		drv_debug_msg(VIDEO_DEBUG_ERROR, "Denoise and Deblock is alternative!\n");
		cell_pipeline_param->filter_pipeline[filter_count++] = -1;
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		goto out;
	}

finished:
	cell_pipeline_param->num_filters = filter_count;

	/* reorder */
	for (i = 1; i < filter_count; ++i)
		for (j = i; j > 0; --j)
			if (cell_pipeline_param->filter_pipeline[j] < cell_pipeline_param->filter_pipeline[j - 1]) {
				/* swap */
				tmp = cell_pipeline_param->filter_pipeline[j];
				cell_pipeline_param->filter_pipeline[j] = cell_pipeline_param->filter_pipeline[j - 1];
				cell_pipeline_param->filter_pipeline[j - 1] = tmp;
			}

	vsp_cmdbuf_insert_command(cmdbuf, &cmdbuf->param_mem, VssProcPipelineParameterCommand,
				  ctx->pipeline_param_offset, sizeof(struct VssProcPipelineParameterBuffer));
out:
	return vaStatus;
}

static VAStatus vsp_set_filter_param(context_VPP_p ctx)
{
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	vsp_cmdbuf_p cmdbuf = ctx->obj_context->vsp_cmdbuf;
	struct VssProcDenoiseParameterBuffer *cell_denoiser_param = (struct VssProcDenoiseParameterBuffer *)cmdbuf->denoise_param_p;
	struct VssProcColorEnhancementParameterBuffer *cell_enhancer_param = (struct VssProcColorEnhancementParameterBuffer *)cmdbuf->enhancer_param_p;
	struct VssProcSharpenParameterBuffer *cell_sharpen_param = (struct VssProcSharpenParameterBuffer *)cmdbuf->sharpen_param_p;
	struct VssProcFrcParameterBuffer *cell_proc_frc_param = (struct VssProcFrcParameterBuffer *)cmdbuf->frc_param_p;
	VAProcFilterParameterBufferBase *cur_param = NULL;
	VAProcFilterParameterBufferFrameRateConversion *frc_param = NULL;
	unsigned int i;
	float ratio;

	for (i = 0; i < ctx->num_filters; ++i) {
		cur_param = (VAProcFilterParameterBufferBase *)ctx->filter_buf[i]->buffer_data;
		switch (cur_param->type) {
			/* FIXME: how to set deblocking filter */
		case VAProcFilterDeblocking:
			cell_denoiser_param->type = VssProcDeblock;
			cell_denoiser_param->value_thr = 20;
			cell_denoiser_param->cnt_thr   = 8;
			cell_denoiser_param->coef      = 9;
			cell_denoiser_param->temp_thr1 = 2;
			cell_denoiser_param->temp_thr2 = 4;
			vsp_cmdbuf_insert_command(cmdbuf, &cmdbuf->param_mem, VssProcDenoiseParameterCommand,
						  ctx->denoise_param_offset, sizeof(struct VssProcDenoiseParameterBuffer));
			break;

		case VAProcFilterNoiseReduction:
			cell_denoiser_param->type = VssProcDegrain;
			cell_denoiser_param->value_thr = 20;
			cell_denoiser_param->cnt_thr   = 8;
			cell_denoiser_param->coef      = 9;
			cell_denoiser_param->temp_thr1 = 2;
			cell_denoiser_param->temp_thr2 = 4;
			vsp_cmdbuf_insert_command(cmdbuf, &cmdbuf->param_mem, VssProcDenoiseParameterCommand,
						  ctx->denoise_param_offset, sizeof(struct VssProcDenoiseParameterBuffer));
			break;

		case VAProcFilterSharpening:
			cell_sharpen_param->quality = 50;
			vsp_cmdbuf_insert_command(cmdbuf, &cmdbuf->param_mem, VssProcSharpenParameterCommand,
						  ctx->sharpen_param_offset, sizeof(struct VssProcSharpenParameterBuffer));
			break;

		case VAProcFilterColorEnhancement:
			cell_enhancer_param->temp_detect  = 100;
			cell_enhancer_param->temp_correct = 100;
			cell_enhancer_param->clip_thr     = 5;
			cell_enhancer_param->mid_thr      = 33;
			cell_enhancer_param->luma_amm     = 100;
			cell_enhancer_param->chroma_amm   = 100;
			vsp_cmdbuf_insert_command(cmdbuf, &cmdbuf->param_mem, VssProcColorEnhancementParameterCommand,
						  ctx->enhancer_param_offset, sizeof(struct VssProcColorEnhancementParameterBuffer));

			break;

		case VAProcFilterFrameRateConversion:
			ctx->frc_buf = ctx->filter_buf[i];

			frc_param = (VAProcFilterParameterBufferFrameRateConversion *)ctx->filter_buf[i]->buffer_data;
			/* FIXME: check if the input fps is in the range of HW capability */
			ratio = frc_param->output_fps / (float)frc_param->input_fps;

			/* fixed to use medium quality */
			cell_proc_frc_param->quality = VssFrcMediumQuality;
			/* cell_proc_frc_param->quality = VssFrcHighQuality; */
			
			if (ratio == 2)
				cell_proc_frc_param->conversion_rate = VssFrc2xConversionRate;
			else if (ratio == 2.5)
				cell_proc_frc_param->conversion_rate = VssFrc2_5xConversionRate;
			else if (ratio == 4)
				cell_proc_frc_param->conversion_rate = VssFrc4xConversionRate;
			else {
				drv_debug_msg(VIDEO_DEBUG_ERROR, "invalid frame rate conversion ratio %f \n", ratio);
				vaStatus = VA_STATUS_ERROR_UNKNOWN;
				goto out;
			}

			vsp_cmdbuf_insert_command(cmdbuf, &cmdbuf->param_mem, VssProcFrcParameterCommand,
						  ctx->frc_param_offset, sizeof(struct VssProcFrcParameterBuffer));
			break;
		default: 
			vaStatus = VA_STATUS_ERROR_UNKNOWN;
			goto out;
		}
	}
out:
	return vaStatus;
}
