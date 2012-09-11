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

/*
 * Authors:
 *    Li Zeng <li.zeng@intel.com>
 */
#include "psb_cmdbuf.h"
#include "psb_surface.h"

struct context_DEC_s {
    object_context_p obj_context; /* back reference */

    uint32_t *cmd_params;
    uint32_t *p_range_mapping_base0;
    uint32_t *p_range_mapping_base1;
    uint32_t *p_slice_params; /* pointer to ui32SliceParams in CMD_HEADER */
    uint32_t *slice_first_pic_last;
    uint32_t *alt_output_flags;

    struct psb_buffer_s aux_line_buffer_vld;
    psb_buffer_p preload_buffer;
    psb_buffer_p slice_data_buffer;


    /* Split buffers */
    int split_buffer_pending;

    /* List of VASliceParameterBuffers */
    object_buffer_p *slice_param_list;
    int slice_param_list_size;
    int slice_param_list_idx;

    unsigned int bits_offset;
    unsigned int SR_flags;

    void (*begin_slice)(struct context_DEC_s *, VASliceParameterBufferBase *);
    void (*process_slice)(struct context_DEC_s *, VASliceParameterBufferBase *);
    void (*end_slice)(struct context_DEC_s *);
    VAStatus (*process_buffer)(struct context_DEC_s *, object_buffer_p);

    struct psb_buffer_s *colocated_buffers;
    int colocated_buffers_size;
    int colocated_buffers_idx;
};

typedef struct context_DEC_s *context_DEC_p;

void vld_dec_FE_state(object_context_p, psb_buffer_p);
void vld_dec_setup_alternative_frame(object_context_p);
VAStatus vld_dec_process_slice_data(context_DEC_p, object_buffer_p);
VAStatus vld_dec_add_slice_param(context_DEC_p, object_buffer_p);
VAStatus vld_dec_allocate_colocated_buffer(context_DEC_p, object_surface_p, uint32_t);
VAStatus vld_dec_CreateContext(context_DEC_p, object_context_p);
void vld_dec_DestroyContext(context_DEC_p);
psb_buffer_p vld_dec_lookup_colocated_buffer(context_DEC_p, psb_surface_p);
void vld_dec_write_kick(object_context_p);
VAStatus vld_dec_RenderPicture( object_context_p, object_buffer_p *, int);

#define AUX_LINE_BUFFER_VLD_SIZE        (1024*152)
