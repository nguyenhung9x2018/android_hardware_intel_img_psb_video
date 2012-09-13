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
 *
 * Authors:
 *    Waldo Bastian <waldo.bastian@intel.com>
 *
 */

#include <sys/types.h>
#include "psb_buffer.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <wsbm/wsbm_manager.h>

#include "psb_drm.h"
#include "psb_def.h"
#include "psb_drv_debug.h"

#include <pnw_cmdbuf.h>

#include "pnw_jpeg.h"
#include "pnw_H264ES.h"
#include "lnc_H264ES.h"
#include "ptg_jpeg.h"

/*
 * Create buffer
 */
VAStatus psb_buffer_create(psb_driver_data_p driver_data,
                           unsigned int size,
                           psb_buffer_type_t type,
                           psb_buffer_p buf
                          )
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int allignment;
    uint32_t placement;
    int ret;

    /* reset rar_handle to NULL */
    buf->rar_handle = 0;
    buf->buffer_ofs = 0;

    buf->type = type;
    buf->driver_data = driver_data; /* only for RAR buffers */
    buf->size = size;
    /* TODO: Mask values are a guess */
    switch (type) {
    case psb_bt_cpu_vpu:
        allignment = 1;
        placement = DRM_PSB_FLAG_MEM_MMU;
        break;
    case psb_bt_cpu_vpu_shared:
        allignment = 1;
        placement = DRM_PSB_FLAG_MEM_MMU | WSBM_PL_FLAG_SHARED;
        break;
    case psb_bt_surface:
        allignment = 0;
        /* Xvideo will share surface buffer, set SHARED flag
         */
        int is_thumbnail = 0;
        char *str;
#ifdef ANDROID        
        if((str = getenv("PSB_VIDEO_THUMBNAIL")))
            is_thumbnail = (gettid() == atoi(str));
#endif
        if (getenv("PSB_VIDEO_SURFACE_MMU") || is_thumbnail) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "Allocate surface from MMU heap\n");
            placement = DRM_PSB_FLAG_MEM_MMU | WSBM_PL_FLAG_SHARED;
        } else {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "Allocate surface from TT heap\n");
            placement = WSBM_PL_FLAG_TT | WSBM_PL_FLAG_SHARED;
        }
        break;
#ifdef PSBVIDEO_MSVDX_DEC_TILING
    case psb_bt_surface_tiling:
            psb__information_message("Allocate tiled surface from TT heap\n");
            placement =  WSBM_PL_FLAG_TT | WSBM_PL_FLAG_SHARED;
            allignment = 2048 * 16; /* Tiled row aligned */
        break;
    case psb_bt_mmu_tiling:
            placement =  DRM_PSB_FLAG_MEM_MMU_TILING | WSBM_PL_FLAG_CACHED | WSBM_PL_FLAG_SHARED;
            allignment = 2048 * 16; /* Tiled row aligned */
        break;
#endif
    case psb_bt_vpu_only:
        allignment = 1;
        placement = DRM_PSB_FLAG_MEM_MMU;
        break;

    case psb_bt_cpu_only:
        allignment = 1;
        placement = WSBM_PL_FLAG_SYSTEM | WSBM_PL_FLAG_CACHED;
        break;
    case psb_bt_camera:
        allignment = 1;
        placement = TTM_PL_FLAG_CI | WSBM_PL_FLAG_SHARED;
        break;
    case psb_bt_rar:
        allignment = 1;
        placement = TTM_PL_FLAG_RAR | WSBM_PL_FLAG_SHARED;
        break;
    default:
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        DEBUG_FAILURE;
        return vaStatus;
    }
    ret = LOCK_HARDWARE(driver_data);
    if (ret) {
        UNLOCK_HARDWARE(driver_data);
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE_RET;
        return vaStatus;
    }

#ifdef VA_EMULATOR
    placement |= WSBM_PL_FLAG_SHARED;
#endif

#ifndef ANDROID
    if(!(placement & WSBM_PL_FLAG_SYSTEM)) {
        //drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: buffer->pl_flags 0x%08x\n", __func__, placement);
        placement &= ~WSBM_PL_MASK_MEM;
        placement |= TTM_PL_FLAG_VRAM;
        //drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: repleace buffer->pl_flags 0x%08x\n", __func__, placement);
    }
#endif

#ifdef MSVDX_VA_EMULATOR
    placement |= WSBM_PL_FLAG_SHARED;
#endif

    if(allignment < 4096)
        allignment = 4096; /* temporily more safe */

    //drv_debug_msg(VIDEO_DEBUG_ERROR, "FIXME: should use geetpagesize() ?\n");
    ret = wsbmGenBuffers(driver_data->main_pool, 1, &buf->drm_buf,
                         allignment, placement);
    if (!buf->drm_buf) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to gen wsbm buffers\n");
        UNLOCK_HARDWARE(driver_data);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    /* here use the placement when gen buffer setted */
    ret = wsbmBOData(buf->drm_buf, size, NULL, NULL, 0);
    UNLOCK_HARDWARE(driver_data);
    if (ret) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to alloc wsbm buffers\n");
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    if (placement & WSBM_PL_FLAG_TT)
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "Create BO with TT placement (%d byte),BO GPU offset hint=0x%08x\n",
                                 size, wsbmBOOffsetHint(buf->drm_buf));

    buf->pl_flags = placement;
    buf->status = psb_bs_ready;
    buf->wsbm_synccpu_flag = 0;

    return VA_STATUS_SUCCESS;
}

/*
 * Create buffer
 */
VAStatus psb_buffer_create_from_ub(psb_driver_data_p driver_data,
                           unsigned int size,
                           psb_buffer_type_t type,
                           psb_buffer_p buf,
                           void * vaddr
                          )
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int allignment;
    uint32_t placement;
    int ret;

    /* reset rar_handle to NULL */
    buf->rar_handle = 0;
    buf->buffer_ofs = 0;

    buf->type = type;
    buf->driver_data = driver_data; /* only for RAR buffers */
    buf->user_ptr = vaddr;

    /* Xvideo will share surface buffer, set SHARED flag
    */
    placement =  DRM_PSB_FLAG_MEM_MMU | WSBM_PL_FLAG_CACHED | WSBM_PL_FLAG_SHARED ;

    ret = LOCK_HARDWARE(driver_data);
    if (ret) {
        UNLOCK_HARDWARE(driver_data);
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE_RET;
        return vaStatus;
    }

    allignment = 4096; /* temporily more safe */
#ifdef PSBVIDEO_MSVDX_DEC_TILING
    if (type == psb_bt_mmu_tiling) {
        placement =  DRM_PSB_FLAG_MEM_MMU_TILING | WSBM_PL_FLAG_CACHED | WSBM_PL_FLAG_SHARED ;
        allignment = 2048 * 16; /* Tiled row aligned */
    }
#endif
    //drv_debug_msg(VIDEO_DEBUG_ERROR, "FIXME: should use geetpagesize() ?\n");
    ret = wsbmGenBuffers(driver_data->main_pool, 1, &buf->drm_buf,
    allignment, placement);
    if (!buf->drm_buf) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to gen wsbm buffers\n");
        UNLOCK_HARDWARE(driver_data);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    /* here use the placement when gen buffer setted */
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "Create BO from user buffer %p, size=%d\n", vaddr, size);

    ret = wsbmBODataUB(buf->drm_buf, size, NULL, NULL, 0, vaddr);
    if (ret) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "Failed to alloc wsbm buffers\n");
        return 1;
    }

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "Create BO from user buffer 0x%08x (%d byte),BO GPU offset hint=0x%08x\n",
    vaddr, size, wsbmBOOffsetHint(buf->drm_buf));

    buf->pl_flags = placement;
    buf->status = psb_bs_ready;
    buf->wsbm_synccpu_flag = 0;

    return VA_STATUS_SUCCESS;
}

/*
 * buffer setstatus
 *
 * Returns 0 on success
 */
int psb_buffer_setstatus(psb_buffer_p buf, uint32_t set_placement, uint32_t clr_placement)
{
    int ret = 0;

    ASSERT(buf);
    ASSERT(buf->driver_data);

    ret = wsbmBOSetStatus(buf->drm_buf, set_placement, clr_placement);
    if (ret == 0)
        buf->pl_flags = set_placement;

    return ret;
}


VAStatus psb_buffer_reference(psb_driver_data_p driver_data,
                              psb_buffer_p buf,
                              psb_buffer_p reference_buf
                             )
{
    int ret = 0;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    memcpy(buf, reference_buf, sizeof(*buf));
    buf->drm_buf = NULL;

    ret = LOCK_HARDWARE(driver_data);
    if (ret) {
        UNLOCK_HARDWARE(driver_data);
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE_RET;
        return vaStatus;
    }

    ret = wsbmGenBuffers(driver_data->main_pool,
                         1,
                         &buf->drm_buf,
                         4096,  /* page alignment */
                         0);
    if (!buf->drm_buf) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to gen wsbm buffers\n");
        UNLOCK_HARDWARE(driver_data);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    ret = wsbmBOSetReferenced(buf->drm_buf, wsbmKBufHandle(wsbmKBuf(reference_buf->drm_buf)));
    UNLOCK_HARDWARE(driver_data);
    if (ret) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to alloc wsbm buffers\n");
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    return VA_STATUS_SUCCESS;
}

VAStatus psb_kbuffer_reference(psb_driver_data_p driver_data,
                               psb_buffer_p buf,
                               int kbuf_handle
                              )
{
    int ret = 0;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    buf->drm_buf = NULL;

    ret = LOCK_HARDWARE(driver_data);
    if (ret) {
        UNLOCK_HARDWARE(driver_data);
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE_RET;
        return vaStatus;
    }

    ret = wsbmGenBuffers(driver_data->main_pool,
                         1,
                         &buf->drm_buf,
                         4096,  /* page alignment */
                         0);
    if (!buf->drm_buf) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to gen wsbm buffers\n");
        UNLOCK_HARDWARE(driver_data);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    ret = wsbmBOSetReferenced(buf->drm_buf, kbuf_handle);
    UNLOCK_HARDWARE(driver_data);
    if (ret) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to alloc wsbm buffers\n");
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
    buf->pl_flags = wsbmBOPlacementHint(buf->drm_buf);

    return VA_STATUS_SUCCESS;
}
/*
 * Destroy buffer
 */
void psb_buffer_destroy(psb_buffer_p buf)
{
    ASSERT(buf);
    if (buf->drm_buf == NULL)
        return;
    if (psb_bs_unfinished != buf->status) {
        ASSERT(buf->driver_data);
        wsbmBOUnreference(&buf->drm_buf);
        if (buf->rar_handle)
            buf->rar_handle = 0;
        buf->driver_data = NULL;
        buf->status = psb_bs_unfinished;
    }
}

/*
 * Map buffer
 *
 * Returns 0 on success
 */
int psb_buffer_map(psb_buffer_p buf, unsigned char **address /* out */)
{
    int ret;

    ASSERT(buf);
    ASSERT(buf->driver_data);

    /* multiple mapping not allowed */
    if (buf->wsbm_synccpu_flag) {
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "Multiple mapping request detected, unmap previous mapping\n");
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "Need to fix application to unmap at first, then request second mapping request\n");

        psb_buffer_unmap(buf);
    }

    /* don't think TG deal with READ/WRITE differently */
    buf->wsbm_synccpu_flag = WSBM_SYNCCPU_READ | WSBM_SYNCCPU_WRITE;
    if (psb_video_trace_fp) {
        wsbmBOWaitIdle(buf->drm_buf, 0);
    } else {
        ret = wsbmBOSyncForCpu(buf->drm_buf, buf->wsbm_synccpu_flag);
        if (ret) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "faild to sync bo for cpu\n");
            return ret;
        }
    }

    if ((buf->type == psb_bt_user_buffer) || buf->handle)
        *address = buf->user_ptr;
    else
        *address = wsbmBOMap(buf->drm_buf, buf->wsbm_synccpu_flag);

    if (*address == NULL) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to map buffer\n");
        return -1;
    }

    return 0;
}

/*
 * Unmap buffer
 *
 * Returns 0 on success
 */
int psb_buffer_unmap(psb_buffer_p buf)
{
    ASSERT(buf);
    ASSERT(buf->driver_data);

    if (buf->wsbm_synccpu_flag)
        (void) wsbmBOReleaseFromCpu(buf->drm_buf, buf->wsbm_synccpu_flag);

    buf->wsbm_synccpu_flag = 0;

    if ((buf->type != psb_bt_user_buffer) && !buf->handle)
        wsbmBOUnmap(buf->drm_buf);

    return 0;
}

void psb__trace_coded(unsigned int *pBuf)
{
    int i, j;
    printf("%s code buffer is\n", __FUNCTION__);
    for (i = 0; i < 6; i++) {
        printf("\t");
        for (j = 0; j < 4; j++) {
             printf("0x%08x, ", pBuf[(i*4) + j]);
        }
        printf("\n");
    }
}

/*
 * Return special data structure for codedbuffer
 *
 * Returns 0 on success
 */
#define CONFIG(id)  ((object_config_p) object_heap_lookup( &driver_data->config_heap, id ))
#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;
int psb_codedbuf_map_mangle(
    VADriverContextP ctx,
    object_buffer_p obj_buffer,
    void **pbuf /* out */
)
{
    object_context_p obj_context = obj_buffer->context;
    INIT_DRIVER_DATA;
    VACodedBufferSegment *p = &obj_buffer->codedbuf_mapinfo[0];
    unsigned char *raw_codedbuf;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    unsigned int next_buf_off;
    int i;

    if (NULL == pbuf) {
        vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
        return vaStatus;
    }

    if (NULL == obj_context) {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE;

        psb_buffer_unmap(obj_buffer->psb_buffer);
        obj_buffer->buffer_data = NULL;

        return vaStatus;
    }

    raw_codedbuf = *pbuf;
    /* reset the mapinfo */
    memset(obj_buffer->codedbuf_mapinfo, 0, sizeof(obj_buffer->codedbuf_mapinfo));

    *pbuf = p = &obj_buffer->codedbuf_mapinfo[0];
#ifdef PSBVIDEO_MRST
    if (IS_MRST(driver_data)) {
        /* one segment */
        p->size = *((unsigned long *) raw_codedbuf); /* 1st DW is the size */
        p->status = *((unsigned long *) raw_codedbuf + 1); /* 2nd DW
                                                        * is rc status */
        p->reserved = 0;
        p->buf = (unsigned char *)((unsigned long *) raw_codedbuf + 4); /* skip 4DWs */
        lnc_H264_append_aux_info(obj_context,
                                 obj_buffer,
                                 (unsigned char *)p->buf,
                                 &(p->size));
    }
#endif
#ifdef PSBVIDEO_MRFL
    if (IS_MRFL(driver_data)) {
        object_config_p obj_config = CONFIG(obj_context->config_id);
        if (NULL == obj_config) {
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            DEBUG_FAILURE;

            psb_buffer_unmap(obj_buffer->psb_buffer);
            obj_buffer->buffer_data = NULL;

            return vaStatus;
        }

        switch (obj_config->profile) {
        case VAProfileMPEG4Simple:
        case VAProfileMPEG4AdvancedSimple:
        case VAProfileMPEG4Main:

        case VAProfileH264Baseline:
        case VAProfileH264Main:
        case VAProfileH264High:
        case VAProfileH264StereoHigh:
        case VAProfileH264ConstrainedBaseline:
        case VAProfileH263Baseline:
            /* 1st segment */
            p->size = *((unsigned long *) raw_codedbuf);
            p->buf = (unsigned char *)((unsigned long *) raw_codedbuf + 16); /* skip 16DWs */
            p->next = NULL;
          psb__trace_coded((unsigned int*)raw_codedbuf);
            break;

        case VAProfileJPEGBaseline:
            /* 3~6 segment */
            ptg_jpeg_AppendMarkers(obj_context, raw_codedbuf);
            next_buf_off = 0;
            /*Max resolution 4096x4096 use 6 segments*/
            for (i = 0; i < PTG_JPEG_MAX_SCAN_NUM + 1; i++) {
                p->size = *(unsigned long *)((unsigned long)raw_codedbuf + next_buf_off);  /* ui32BytesUsed in HEADER_BUFFER*/
                p->buf = (unsigned char *)((unsigned long *)((unsigned long)raw_codedbuf + next_buf_off) + 4);  /* skip 4DWs (HEADER_BUFFER) */
                next_buf_off = *((unsigned long *)((unsigned long)raw_codedbuf + next_buf_off) + 3);  /* ui32Reserved3 in HEADER_BUFFER*/

                drv_debug_msg(VIDEO_DEBUG_GENERAL, "JPEG coded buffer segment %d size: %d\n", i, p->size);
                drv_debug_msg(VIDEO_DEBUG_GENERAL, "JPEG coded buffer next segment %d offset: %d\n", i + 1, next_buf_off);

                if (next_buf_off == 0) {
                    p->next = NULL;
                    break;
                } else
                    p->next = &p[1];
                p++;
            }
            break;

        default:
            drv_debug_msg(VIDEO_DEBUG_ERROR, "unexpected case\n");

            psb_buffer_unmap(obj_buffer->psb_buffer);
            obj_buffer->buffer_data = NULL;
            break;
        }
    }
#endif
#ifdef PSBVIDEO_MFLD
    if (IS_MFLD(driver_data)){ /* MFLD */
        object_config_p obj_config = CONFIG(obj_context->config_id);

        if (NULL == obj_config) {
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            DEBUG_FAILURE;

            psb_buffer_unmap(obj_buffer->psb_buffer);
            obj_buffer->buffer_data = NULL;

            return vaStatus;
        }

        if (VAProfileJPEGBaseline != obj_config->profile
            && (*((unsigned long *) raw_codedbuf + 1) & SKIP_NEXT_FRAME) != 0) {
            /*Set frame skip flag*/
            pnw_set_frame_skip_flag(obj_context);
        }
        switch (obj_config->profile) {
        case VAProfileMPEG4Simple:
        case VAProfileMPEG4AdvancedSimple:
        case VAProfileMPEG4Main:
            /* one segment */
            p->size = *((unsigned long *) raw_codedbuf);
            p->buf = (unsigned char *)((unsigned long *) raw_codedbuf + 4); /* skip 4DWs */
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "coded buffer size %d\n", p->size);
            break;

        case VAProfileH264Baseline:
        case VAProfileH264Main:
        case VAProfileH264High:
        case VAProfileH264ConstrainedBaseline:
            i = 0;
            if (pnw_get_parallel_core_number(obj_context) == 2) {
            /*The second part of coded buffer which generated by core 2 is the
             * first part of encoded clip, while the first part of coded buffer
             * is the second part of encoded clip.*/
            next_buf_off = ~0xf & (obj_buffer->size / pnw_get_parallel_core_number(obj_context));
            p[i].next = &p[i + 1];
            p[i].size = *(unsigned long *)((unsigned long)raw_codedbuf + next_buf_off);
            p[i].buf = (unsigned char *)(((unsigned long *)((unsigned long)raw_codedbuf + next_buf_off)) + 4); /* skip 4DWs */

            if (GET_CODEDBUF_INFO(SLICE_NUM, obj_buffer->codedbuf_aux_info) <= 2 &&
                GET_CODEDBUF_INFO(NONE_VCL_NUM, obj_buffer->codedbuf_aux_info) == 0) {
                p[i].status =  VA_CODED_BUF_STATUS_SINGLE_NALU;
                drv_debug_msg(VIDEO_DEBUG_GENERAL, "Only VCL NAL in this segment %i of coded buffer\n",
                    i);
            }
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "2nd segment coded buffer offset: 0x%08x,  size: %d\n",
                next_buf_off, p[i].size);
            i++;
            }
            /* 1st segment */
            p[i].size = *((unsigned long *) raw_codedbuf);
            p[i].buf = (unsigned char *)((unsigned long *) raw_codedbuf + 4); /* skip 4DWs */
            if (GET_CODEDBUF_INFO(SLICE_NUM, obj_buffer->codedbuf_aux_info) <= 2 &&
                GET_CODEDBUF_INFO(NONE_VCL_NUM, obj_buffer->codedbuf_aux_info) == 0) {
            p[i].status =  VA_CODED_BUF_STATUS_SINGLE_NALU;
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "Only VCL NAL in this segment %i of coded buffer\n",
                i);
            }
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "1st segment coded buffer size %d\n", p[i].size);

            break;

        case VAProfileH263Baseline:
                /* one segment */
            p->size = *((unsigned long *) raw_codedbuf);
            p->buf = (unsigned char *)((unsigned long *) raw_codedbuf + 4); /* skip 4DWs */
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "coded buffer size %d\n", p->size);
            break;

        case VAProfileJPEGBaseline:
            /* 3~6 segment
                 */
            pnw_jpeg_AppendMarkers(obj_context, raw_codedbuf);
            next_buf_off = 0;
            /*Max resolution 4096x4096 use 6 segments*/
            for (i = 0; i < PNW_JPEG_MAX_SCAN_NUM + 1; i++) {
                p->size = *(unsigned long *)((unsigned long)raw_codedbuf + next_buf_off);
                p->buf = (unsigned char *)((unsigned long *)((unsigned long)raw_codedbuf + next_buf_off) + 4);  /* skip 4DWs */
                next_buf_off = *((unsigned long *)((unsigned long)raw_codedbuf + next_buf_off) + 3);

                drv_debug_msg(VIDEO_DEBUG_GENERAL, "JPEG coded buffer segment %d size: %d\n", i, p->size);
                drv_debug_msg(VIDEO_DEBUG_GENERAL, "JPEG coded buffer next segment %d offset: %d\n", i + 1, next_buf_off);

                if (next_buf_off == 0) {
                    p->next = NULL;
                    break;
                } else
                    p->next = &p[1];
                p++;
            }
            break;

        default:
            drv_debug_msg(VIDEO_DEBUG_ERROR, "unexpected case\n");

            psb_buffer_unmap(obj_buffer->psb_buffer);
            obj_buffer->buffer_data = NULL;
            break;
        }
    }
#endif

    return 0;
}

int psb_buffer_sync(psb_buffer_p buf)
{
    int ret;

    ASSERT(buf);
    ASSERT(buf->driver_data);

    ret = wsbmBOSyncForCpu(buf->drm_buf, buf->wsbm_synccpu_flag);
    if (ret) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "faild to sync bo for cpu\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    return VA_STATUS_SUCCESS;
}
