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

#include "psb_buffer.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <wsbm/wsbm_manager.h>

#include "psb_drm.h"
#include "psb_def.h"

#include <pnw_cmdbuf.h>

#include "pnw_jpeg.h"
/*
 * Create buffer
 */
VAStatus psb_buffer_create( psb_driver_data_p driver_data,
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

    /* TODO: Mask values are a guess */
    switch (type)
    {
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
                if (getenv("PSB_VIDEO_SURFACE_MMU")) {
                    psb__information_message("Allocate surface from MMU heap\n");
                    placement = DRM_PSB_FLAG_MEM_MMU | WSBM_PL_FLAG_SHARED;
                } else {
                    psb__information_message("Allocate surface from TT heap\n");
                    placement = WSBM_PL_FLAG_TT | WSBM_PL_FLAG_SHARED;
                }
		break;
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
		placement = DRM_PSB_FLAG_MEM_CI | WSBM_PL_FLAG_SHARED;
		break;
            case psb_bt_rar:
		allignment = 1;
		placement = DRM_PSB_FLAG_MEM_RAR | WSBM_PL_FLAG_SHARED;
		break;
            default:
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		DEBUG_FAILURE;
		return vaStatus;
    }
    ret = LOCK_HARDWARE(driver_data);
    if (ret)
    {
        UNLOCK_HARDWARE(driver_data);
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE_RET;
        return vaStatus;
    }

#ifdef VA_EMULATOR
    placement |= WSBM_PL_FLAG_SHARED;
#endif

#ifdef MSVDX_VA_EMULATOR
    placement |= WSBM_PL_FLAG_SHARED;
#endif

    allignment = 4096; /* temporily more safe */
    
    //psb__error_message("FIXME: should use geetpagesize() ?\n");
    ret = wsbmGenBuffers(driver_data->main_pool, 1, &buf->drm_buf,
                         allignment, placement);
    if (!buf->drm_buf) {
        psb__error_message("failed to gen wsbm buffers\n");
        UNLOCK_HARDWARE(driver_data);		
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    /* here use the placement when gen buffer setted */
    ret = wsbmBOData(buf->drm_buf, size, NULL, NULL, 0);
    UNLOCK_HARDWARE(driver_data);	
    if (ret) {
        psb__error_message("failed to alloc wsbm buffers\n");
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    if (placement & WSBM_PL_FLAG_TT)
      psb__information_message("Create BO with TT placement (%d byte),BO GPU offset hint=0x%08x\n",
			       size, wsbmBOOffsetHint(buf->drm_buf));

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
int psb_buffer_setstatus( psb_buffer_p buf, uint32_t set_placement, uint32_t clr_placement)
{
    int ret = 0;
    
    ASSERT(buf);
    ASSERT(buf->driver_data);

    ret = wsbmBOSetStatus(buf->drm_buf, set_placement, clr_placement);
    if (ret == 0)
        buf->pl_flags = set_placement;
    
    return ret;
}


VAStatus psb_buffer_reference( psb_driver_data_p driver_data,
                            psb_buffer_p buf,
                            psb_buffer_p reference_buf
                            )
{
    int ret = 0;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    
    memcpy(buf, reference_buf, sizeof(*buf));
    buf->drm_buf = NULL;
    
    ret = LOCK_HARDWARE(driver_data);
    if (ret)
    {
        UNLOCK_HARDWARE(driver_data);
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE_RET;
        return vaStatus;
    }
    
    ret = wsbmGenBuffers( driver_data->main_pool, 
                    1, 
                    &buf->drm_buf,
                    4096,  /* page alignment */
                    0 );
    if (!buf->drm_buf) {
        psb__error_message("failed to gen wsbm buffers\n");
        UNLOCK_HARDWARE(driver_data);		
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
    
    ret = wsbmBOSetReferenced(buf->drm_buf, wsbmKBufHandle(wsbmKBuf(reference_buf->drm_buf)));
    UNLOCK_HARDWARE(driver_data);	
    if (ret) {
        psb__error_message("failed to alloc wsbm buffers\n");
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    return VA_STATUS_SUCCESS;
}


/*
 * Destroy buffer
 */   
void psb_buffer_destroy( psb_buffer_p buf )
{
    ASSERT(buf);
    if (buf->drm_buf == NULL)
	    return;
    if (psb_bs_unfinished != buf->status)
    {
        ASSERT(buf->driver_data);
	wsbmBOUnreference(&buf->drm_buf);
        if (buf->rar_handle)
            psb_buffer_destroy_rar(buf->driver_data, buf);
        buf->driver_data = NULL;
        buf->status = psb_bs_unfinished;
    }
}

/*
 * Map buffer
 *
 * Returns 0 on success
 */
int psb_buffer_map( psb_buffer_p buf, void **address /* out */ )
{
    int ret;

    ASSERT(buf);
    ASSERT(buf->driver_data);

    /* multiple mapping not allowed */    
    if (buf->wsbm_synccpu_flag) {
        psb__information_message("Multiple mapping request detected, unmap previous mapping\n");
        psb__information_message("Need to fix application to unmap at first, then request second mapping request\n");

	psb_buffer_unmap(buf);
    }
    
    /* don't think TG deal with READ/WRITE differently */
    buf->wsbm_synccpu_flag = WSBM_SYNCCPU_READ | WSBM_SYNCCPU_WRITE;
#ifdef DEBUG_TRACE
    wsbmBOWaitIdle(buf->drm_buf, 0);
#else
    ret = wsbmBOSyncForCpu(buf->drm_buf, buf->wsbm_synccpu_flag);
    if (ret) {
        psb__error_message("faild to sync bo for cpu\n");
        return ret;
    }
#endif

    if (buf->type == psb_bt_user_buffer)
        *address = buf->user_ptr;
    else
        *address = wsbmBOMap(buf->drm_buf, buf->wsbm_synccpu_flag);
    
    if (*address == NULL) {
        psb__error_message("failed to map buffer\n");
        return -1;
    }
        
    return 0;

}

/*
 * Unmap buffer
 *
 * Returns 0 on success
 */
int psb_buffer_unmap( psb_buffer_p buf )
{
    ASSERT(buf);
    ASSERT(buf->driver_data);

    if (buf->wsbm_synccpu_flag)
        (void) wsbmBOReleaseFromCpu(buf->drm_buf, buf->wsbm_synccpu_flag);
    
    buf->wsbm_synccpu_flag = 0;

    if (buf->type != psb_bt_user_buffer)
        wsbmBOUnmap(buf->drm_buf);
    
    return 0;
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
    void *raw_codedbuf;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    unsigned int next_buf_off; 
    int i;
    
    if (NULL == pbuf)
    {
	vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
        return vaStatus;
    }

    if (NULL == obj_context)
    {
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
    if (IS_MRST(driver_data)) {
        /* one segment */
        p->size = *((unsigned long *) raw_codedbuf); /* 1st DW is the size */
        p->buf = (void *)((unsigned long *) raw_codedbuf + 4); /* skip 4DWs */
    } else { /* MFLD */
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
	    /* one segment */
	    p->size = *((unsigned long *) raw_codedbuf);
	    p->buf = (void *)((unsigned long *) raw_codedbuf + 4); /* skip 4DWs */
	    psb__information_message("coded buffer size %d\n", p->size);
	    break;

        case VAProfileH264Baseline:
        case VAProfileH264Main:
        case VAProfileH264High:
            /* 1st segment */
            p->size = *((unsigned long *) raw_codedbuf);
            p->buf = (void *)((unsigned long *) raw_codedbuf + 4); /* skip 4DWs */

	    psb__information_message("1st segment coded buffer size %d\n", p->size);
	    if (pnw_get_parallel_core_number(obj_context) == 2)
	    {
		/*The second part of coded buffer which generated by core 2 is the 
		 * first part of encoded clip, while the first part of coded buffer
		 * is the second part of encoded clip.*/
		next_buf_off = ~0xf & (obj_buffer->size / pnw_get_parallel_core_number(obj_context)); 
		p->next = &p[1];
		p[1].size = p->size;
		p[1].buf = p->buf;
		p[1].next = NULL;
		p->size = *(unsigned long *)((unsigned long)raw_codedbuf + next_buf_off); 
		p->buf = (void *)(((unsigned long *)((unsigned long)raw_codedbuf + next_buf_off)) + 4); /* skip 4DWs */
		psb__information_message("2nd segment coded buffer offset: 0x%08x,  size: %d\n", 
			next_buf_off, p->size);
	    }
	    else
		p->next = NULL;
            break;

        case VAProfileH263Baseline:
	    /* one segment */
	    p->size = *((unsigned long *) raw_codedbuf);
	    p->buf = (void *)((unsigned long *) raw_codedbuf + 4); /* skip 4DWs */
	    psb__information_message("coded buffer size %d\n", p->size);
	    break;

        case VAProfileJPEGBaseline:
	    /* 3~6 segment
             */
	    pnw_jpeg_AppendMarkers(obj_context, raw_codedbuf);
	    next_buf_off = 0; 
	    /*Max resolution 4096x4096 use 6 segments*/
	    for (i = 0; i < PNW_JPEG_MAX_SCAN_NUM + 1; i++)
	    {
		p->size = *(unsigned long *)((unsigned long)raw_codedbuf + next_buf_off);
		p->buf = (void *)((unsigned long *) ((unsigned long)raw_codedbuf + next_buf_off) + 4); /* skip 4DWs */
		next_buf_off = *((unsigned long *) ((unsigned long)raw_codedbuf + next_buf_off) + 3); 

		psb__information_message("JPEG coded buffer segment %d size: %d\n", i, p->size);
		psb__information_message("JPEG coded buffer next segment %d offset: %d\n", i+1, next_buf_off);

		if (next_buf_off == 0)
		{
		    p->next = NULL;
		    break;
		}
		else
		    p->next = &p[1];
		p++;
	    }
	    break;
            
        default:
            psb__error_message("unexpected case\n");
            
            psb_buffer_unmap(obj_buffer->psb_buffer);
            obj_buffer->buffer_data = NULL;
            break;
        }
    }
    
    return 0;
}
