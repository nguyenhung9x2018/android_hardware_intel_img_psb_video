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

#ifndef _VSP_VPP_H_
#define _VSP_VPP_H_

#include "psb_drv_video.h"
#include "vsp_fw.h"

struct context_VPP_s {
	object_context_p obj_context; /* back reference */

	uint32_t profile; // ENTDEC BE_PROFILE & FE_PROFILE
	uint32_t profile_idc; // BE_PROFILEIDC

	struct psb_buffer_s *context_buf;

	VABufferID *filters;
	unsigned int num_filters;

	enum vsp_format format;

	object_buffer_p filter_buf[VssProcPipelineMaxNumFilters];
	object_buffer_p frc_buf;

	unsigned int param_sz;
	unsigned int pic_param_sz;
	unsigned int pic_param_offset;
	unsigned int end_param_sz;
	unsigned int end_param_offset;
	unsigned int pipeline_param_sz;
	unsigned int pipeline_param_offset;
	unsigned int denoise_param_sz;
	unsigned int denoise_param_offset;
	unsigned int enhancer_param_sz;
	unsigned int enhancer_param_offset;
	unsigned int sharpen_param_sz;
	unsigned int sharpen_param_offset;
	unsigned int frc_param_sz;
	unsigned int frc_param_offset;
};

typedef struct context_VPP_s *context_VPP_p;

extern struct format_vtable_s vsp_VPP_vtable;

extern VAStatus vsp_QueryVideoProcFilters(
        VADriverContextP    ctx,
        VAContextID         context,
        VAProcFilterType   *filters,
        unsigned int       *num_filters
	);

extern VAStatus vsp_QueryVideoProcFilterCaps(
        VADriverContextP    ctx,
        VAContextID         context,
        VAProcFilterType    type,
        void               *filter_caps,
        unsigned int       *num_filter_caps
	);

extern VAStatus vsp_QueryVideoProcPipelineCaps(
	VADriverContextP    ctx,
        VAContextID         context,
        VABufferID         *filters,
        unsigned int        num_filters,
        VAProcPipelineCaps *pipeline_caps
    );

#endif /* _VSS_VPP_H_ */
