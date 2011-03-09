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

#ifndef _LNC_CMDBUF_H_
#define _LNC_CMDBUF_H_

#include "psb_drv_video.h"
#include "psb_surface.h"
#include "psb_buffer.h"
#include "psb_drm.h"

#include <stdint.h>


struct lnc_cmdbuf_s {
    struct psb_buffer_s buf;
    unsigned int size;

    /* Relocation records */
    void *reloc_base;
    struct drm_psb_reloc *reloc_idx;

    /* CMD stream data */
    int cmd_count;
    void *cmd_base;
    void *cmd_start;
    uint32_t *cmd_idx;
    uint32_t *cmd_idx_saved_frameskip; /* idx saved for frameskip redo */

    /* all frames share one topaz param buffer which contains InParamBase
     * AboveParam/BellowParam, and the buffer allocated when the context is created
     */
    struct psb_buffer_s *topaz_in_params_I;
    struct psb_buffer_s *topaz_in_params_P;
    struct psb_buffer_s *topaz_above_bellow_params;
    void *topaz_in_params_I_p;
    void *topaz_in_params_P_p;
    void *topaz_above_bellow_params_p;

    /* Every frame has its own PIC_PARAMS, SLICE_PARAMS and HEADER mem
     */

    /* PicParams: */
    struct psb_buffer_s pic_params;
    void *pic_params_p;

    /* SeqHeaderMem PicHeaderMem EOSeqHeaderMem  EOStreamHeaderMem SliceHeaderMem[MAX_SLICES_PER_PICTURE]*/
    struct psb_buffer_s header_mem;
    void *header_mem_p;

    /*SliceParams[MAX_SLICES_PER_PICTURE] */
    struct psb_buffer_s slice_params;
    void *slice_params_p;

    /* Referenced buffers */
    psb_buffer_p *buffer_refs;
    int buffer_refs_count;
    int buffer_refs_allocated;

};

typedef struct lnc_cmdbuf_s *lnc_cmdbuf_p;

/*
 * Create command buffer
 */
VAStatus lnc_cmdbuf_create(object_context_p obj_context,
                           psb_driver_data_p driver_data,
                           lnc_cmdbuf_p cmdbuf
                          );

/*
 * Destroy buffer
 */
void lnc_cmdbuf_destroy(lnc_cmdbuf_p cmdbuf);

/*
 * Reset buffer & map
 *
 * Returns 0 on success
 */
int lnc_cmdbuf_reset(lnc_cmdbuf_p cmdbuf);

/*
 * Unmap buffer
 *
 * Returns 0 on success
 */
int lnc_cmdbuf_unmap(lnc_cmdbuf_p cmdbuf);

/*
 * Reference an addtional buffer "buf" in the command stream
 * Returns a reference index that can be used to refer to "buf" in
 * relocation records, on error -1 is returned.
 */
int lnc_cmdbuf_buffer_ref(lnc_cmdbuf_p cmdbuf, psb_buffer_p buf);

/* Creates a relocation record for a DWORD in the mapped "cmdbuf" at address
 * "addr_in_cmdbuf"
 * The relocation is based on the device virtual address of "ref_buffer"
 * "buf_offset" is be added to the device virtual address, and the sum is then
 * right shifted with "align_shift".
 * "mask" determines which bits of the target DWORD will be updated with the so
 * constructed address. The remaining bits will be filled with bits from "background".
 */
void lnc_cmdbuf_add_relocation(lnc_cmdbuf_p cmdbuf,
                               uint32_t *addr_in_dst_buffer,/*addr of dst_buffer for the DWORD*/
                               psb_buffer_p ref_buffer,
                               uint32_t buf_offset,
                               uint32_t mask,
                               uint32_t background,
                               uint32_t align_shift,
                               uint32_t dst_buffer, /*Index of the list refered by cmdbuf->buffer_refs */
                               uint32_t *start_of_dst_buffer);

#define RELOC_CMDBUF(dest, offset, buf)	lnc_cmdbuf_add_relocation(cmdbuf, (uint32_t*)(dest), buf, offset, 0XFFFFFFFF, 0, 0, 0, cmdbuf->cmd_start)

/* do relocation in PIC_PARAMS: src/dst Y/UV base, InParamsBase, CodeBase, BellowParamsBase, AboveParamsBase */
#define RELOC_PIC_PARAMS(dest, offset, buf)	lnc_cmdbuf_add_relocation(cmdbuf, (uint32_t*)(dest), buf, offset, 0XFFFFFFFF, 0, 0, 1, (uint32_t *)cmdbuf->pic_params_p)

/* do relocation in SLICE_PARAMS: reference Y/UV base,CodedData */
#define RELOC_SLICE_PARAMS(dest, offset, buf)	lnc_cmdbuf_add_relocation(cmdbuf, (uint32_t*)(dest), buf, offset, 0XFFFFFFFF, 0, 0, 2,(uint32_t *)cmdbuf->slice_params_p)

/* operation number is inserted by DRM */
#define lnc_cmdbuf_insert_command(cmdbuf,cmdhdr,size,hint)              \
    do { *cmdbuf->cmd_idx++ = ((cmdhdr) << 1) | ((size)<<8) | ((hint)<<16); } while(0)

#define lnc_cmdbuf_insert_command_param(cmdbuf,param)   \
    do { *cmdbuf->cmd_idx++ = param; } while(0)


/*
 * Advances "obj_context" to the next cmdbuf
 *
 * Returns 0 on success
 */
int lnc_context_get_next_cmdbuf(object_context_p obj_context);

/*
 * Submits the current cmdbuf
 *
 * Returns 0 on success
 */
int lnc_context_submit_cmdbuf(object_context_p obj_context);

/*
 * Get a encode surface FRAMESKIP flag, and store it into frame_skip argument
 *
 * Returns 0 on success
 */
int lnc_surface_get_frameskip(psb_driver_data_p driver_data, psb_surface_p psb_surface, int *frame_skip);

/*
 * Flushes the pending cmdbuf
 *
 * Return 0 on success
 */
int lnc_context_flush_cmdbuf(object_context_p obj_context);

#endif /* _LNC_CMDBUF_H_ */

