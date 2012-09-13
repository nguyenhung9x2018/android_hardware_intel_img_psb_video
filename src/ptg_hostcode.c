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
 *    Elaine Wang <elaine.wang@intel.com>
 *    Zeng Li <zeng.li@intel.com>
 *    Edward Lin <edward.lin@intel.com>
 *
 */

#include "psb_drv_video.h"
//#include "ptg_H263ES.h"
#include "ptg_hostheader.h"
#include "ptg_hostcode.h"
#include "psb_def.h"
#include "psb_drv_debug.h"
#include "psb_cmdbuf.h"
#include <stdio.h>
#include "psb_output.h"
#include "ptg_picmgmt.h"
#include "ptg_hostbias.h"
#include <wsbm/wsbm_manager.h>

#include "hwdefs/topazhp_core_regs.h"
#include "hwdefs/topazhp_multicore_regs_old.h"
#include "hwdefs/topaz_db_regs.h"
#include "hwdefs/topaz_vlc_regs.h"
#include "hwdefs/mvea_regs.h"
#include "hwdefs/topazhp_default_params.h"
#ifdef _TOPAZHP_PDUMP_
#include "ptg_trace.h"
#endif

#define ALIGN_TO(value, align) ((value + align - 1) & ~(align - 1))
#define PAGE_ALIGN(value) ALIGN_TO(value, 4096)
#define DEFAULT_MVCALC_CONFIG   ((0x00040303)|(MASK_TOPAZHP_CR_MVCALC_JITTER_POINTER_RST))
#define DEFAULT_MVCALC_COLOCATED        (0x00100100)
#define MVEA_MV_PARAM_REGION_SIZE 16
#define MVEA_ABOVE_PARAM_REGION_SIZE 96
#define QUANT_LISTS_SIZE                (224)
#define _1080P_30FPS (((1920*1088)/256)*30)
#define ptg_align_KB(x)  (((x) + (IMG_UINT32)(0xfff)) & (~(IMG_UINT32)(0xfff)))
#define ptg_align_64(X)  (((X)+63) &~63)
#define ptg_align_4(X)  (((X)+3) &~3)

#define DEFAULT_CABAC_DB_MARGIN    (0x190)
#define NOT_USED_BY_TOPAZ 0

#ifdef _TOPAZHP_PDUMP_ALL_
#define _PDUMP_FUNC_
#define _PDUMP_SLICE_
#define _TOPAZHP_CMDBUF_
#endif

#define _PDUMP_BFRAME_

#ifdef _TOPAZHP_CMDBUF_
static void ptg__trace_cmdbuf_words(ptg_cmdbuf_p cmdbuf)
{
    int i = 0;
    IMG_UINT32 *ptmp = (IMG_UINT32 *)(cmdbuf->cmd_start);
    IMG_UINT32 *pend = (IMG_UINT32 *)(cmdbuf->cmd_idx);
    do {
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: command words [%d] = 0x%08x\n", __FUNCTION__, i++, (unsigned int)(*ptmp++));
    } while(ptmp < pend);
    return ;
}
#endif


static IMG_UINT32 ptg__get_codedbuffer_size(
    IMG_STANDARD eStandard,
    IMG_UINT16 ui16MBWidth,
    IMG_UINT16 ui16MBHeight,
    IMG_RC_PARAMS * psRCParams
)
{
    if (eStandard == IMG_STANDARD_H264) {
        // allocate based on worst case qp size
        return ((IMG_UINT32)ui16MBWidth * (IMG_UINT32)ui16MBHeight * 400);
    }

    if (psRCParams->ui32InitialQp <= 5)
        return ((IMG_UINT32)ui16MBWidth * (IMG_UINT32)ui16MBHeight * 1600);

    return ((IMG_UINT32)ui16MBWidth * (IMG_UINT32)ui16MBHeight * 900);
}


static IMG_UINT32 ptg__get_codedbuf_size_according_bitrate(
    IMG_RC_PARAMS * psRCParams
)
{
    return ((psRCParams->ui32BitsPerSecond + psRCParams->ui32FrameRate / 2) / psRCParams->ui32FrameRate) * 2;
}

static IMG_UINT32 ptg__get_buffer_size(IMG_UINT32 src_size)
{
    return (src_size + 0x1000) & (~0xfff);
}

static inline VAStatus ptg__alloc_init_buffer(
    psb_driver_data_p driver_data,
    unsigned int size,
    psb_buffer_type_t type,
    psb_buffer_p buf)
{
    IMG_UINT32 pTry;
    unsigned char *pch_virt_addr;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    vaStatus = psb_buffer_create(driver_data, size, type, buf);
    if (VA_STATUS_SUCCESS != vaStatus) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "alloc mem params buffer");
        return vaStatus;
    }

    ptg_cmdbuf_set_phys(&pTry, 1, buf, 0, 0);
    
    vaStatus = psb_buffer_map(buf, &pch_virt_addr);
    if (vaStatus) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "map buf\n");
        psb_buffer_destroy(buf);
        return vaStatus;
    } else {
        memset(pch_virt_addr, 0, size);
#ifdef _TOPAZHP_VIR_ADDR_
        psb_buffer_unmap(buf); 
        if (vaStatus) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "unmap buf\n");
            psb_buffer_destroy(buf);
            return vaStatus;
        }
#endif
        
    }
    return vaStatus;
}

static VAStatus ptg__alloc_context_buffer(context_ENC_p ctx, unsigned char is_JPEG, unsigned int stream_id)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    IMG_UINT32 ui32_pic_width, ui32_pic_height;
    IMG_UINT32 ui32_mb_width, ui32_mb_height;
    IMG_INT32   i32_num_pipes;
    IMG_RC_PARAMS *psRCParams = &(ctx->sRCParams);
    psb_driver_data_p ps_driver_data = ctx->obj_context->driver_data;
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[stream_id]);
    context_ENC_mem_size *ps_mem_size = &(ctx->ctx_mem_size);
    

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: start\n", __FUNCTION__);

    if (ctx->eStandard == IMG_STANDARD_H264) {
        i32_num_pipes = MIN(ctx->i32NumPipes, ctx->ui8SlicesPerPicture);
    } else {
        i32_num_pipes = 1;
    }

    ctx->i32NumPipes = i32_num_pipes;
    ctx->i32PicNodes  = (psRCParams->b16Hierarchical ? MAX_REF_B_LEVELS : 0) + 4;
    ctx->i32MVStores = (ctx->i32PicNodes * 2);
    ctx->i32CodedBuffers = i32_num_pipes * (ctx->bIsInterlaced ? 3 : 2);
    ctx->ui8SlotsInUse = psRCParams->ui16BFrames + 2;

    if (0 != is_JPEG) {
        ctx->jpeg_pic_params_size = (sizeof(JPEG_MTX_QUANT_TABLE) + 0x3f) & (~0x3f);
        ctx->jpeg_header_mem_size = (sizeof(JPEG_MTX_DMA_SETUP) + 0x3f) & (~0x3f);
        ctx->jpeg_header_interface_mem_size = ctx->jpeg_header_mem_size;

        //write back region
        ps_mem_size->writeback = ptg_align_KB(COMM_WB_DATA_BUF_SIZE);
        ptg__alloc_init_buffer(ps_driver_data, WB_FIFO_SIZE * ps_mem_size->writeback, psb_bt_cpu_vpu, &(ctx->bufs_writeback));

        drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: end\n", __FUNCTION__);
        return vaStatus;
    }

    /* width and height should be source surface's w and h or ?? */
    ui32_pic_width = ctx->obj_context->picture_width;
    ui32_mb_width = (ctx->obj_context->picture_width + 15) >> 4;
    ui32_pic_height = ctx->obj_context->picture_height;
    ui32_mb_height = (ctx->obj_context->picture_height + 15) >> 4;

    //command buffer use
    ps_mem_size->pic_template = ps_mem_size->slice_template = 
    ps_mem_size->sei_header = ps_mem_size->seq_header = ptg_align_KB(PTG_HEADER_SIZE);
    ptg__alloc_init_buffer(ps_driver_data, ps_mem_size->seq_header,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_seq_header));
    if (ctx->bEnableMVC)
        ptg__alloc_init_buffer(ps_driver_data, ps_mem_size->seq_header,
                                        psb_bt_cpu_vpu, &(ps_mem->bufs_sub_seq_header));

    ptg__alloc_init_buffer(ps_driver_data, 4 * ps_mem_size->pic_template,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_pic_template));
    ptg__alloc_init_buffer(ps_driver_data, NUM_SLICE_TYPES * ps_mem_size->slice_template,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_slice_template));

    ps_mem_size->mtx_context = ptg_align_KB(MTX_CONTEXT_SIZE);
    ptg__alloc_init_buffer(ps_driver_data, ps_mem_size->mtx_context,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_mtx_context));

    //gop header
    ps_mem_size->flat_gop = ps_mem_size->hierar_gop = ptg_align_KB(64);
    ptg__alloc_init_buffer(ps_driver_data, ps_mem_size->flat_gop,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_flat_gop));
    ptg__alloc_init_buffer(ps_driver_data, ps_mem_size->hierar_gop,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_hierar_gop));

    //above params
    ps_mem_size->above_params = ptg_align_KB(MVEA_ABOVE_PARAM_REGION_SIZE * ptg_align_64(ui32_mb_width));
    ptg__alloc_init_buffer(ps_driver_data, ctx->i32NumPipes * ps_mem_size->above_params,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_above_params));

    //ctx->mv_setting_btable_size = ptg_align_KB(MAX_BFRAMES * (ptg_align_64(sizeof(IMG_MV_SETTINGS) * MAX_BFRAMES)));
    ps_mem_size->mv_setting_btable = ptg_align_KB(MAX_BFRAMES * MV_ROW_STRIDE);
    ptg__alloc_init_buffer(ps_driver_data, ps_mem_size->mv_setting_btable,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_mv_setting_btable));
    
    ps_mem_size->mv_setting_hierar = ptg_align_KB(MAX_BFRAMES * sizeof(IMG_MV_SETTINGS));
    ptg__alloc_init_buffer(ps_driver_data, ps_mem_size->mv_setting_hierar,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_mv_setting_hierar));

    //colocated params
    ps_mem_size->colocated = ptg_align_KB(MVEA_MV_PARAM_REGION_SIZE * ptg_align_4(ui32_mb_width * ui32_mb_height));
    ptg__alloc_init_buffer(ps_driver_data, ctx->i32PicNodes * ps_mem_size->colocated,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_colocated));

    ps_mem_size->interview_mv = ps_mem_size->mv = ps_mem_size->colocated;
    ptg__alloc_init_buffer(ps_driver_data, ctx->i32MVStores * ps_mem_size->mv,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_mv));

    if (ctx->bEnableMVC) {
        ptg__alloc_init_buffer(ps_driver_data, 2 * ps_mem_size->interview_mv,
                                        psb_bt_cpu_vpu, &(ps_mem->bufs_interview_mv));
    }

    //write back region
    ps_mem_size->writeback = ptg_align_KB(COMM_WB_DATA_BUF_SIZE);
    ptg__alloc_init_buffer(ps_driver_data, WB_FIFO_SIZE * ps_mem_size->writeback,
                                    psb_bt_cpu_vpu, &(ctx->bufs_writeback));

    ps_mem_size->slice_map = ptg_align_KB(0x1500); //(1 + MAX_SLICESPERPIC * 2 + 15) & ~15);
    ptg__alloc_init_buffer(ps_driver_data, ctx->ui8SlotsInUse * ps_mem_size->slice_map,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_slice_map));

    ps_mem_size->weighted_prediction = ptg_align_KB(sizeof(WEIGHTED_PREDICTION_VALUES));
    ptg__alloc_init_buffer(ps_driver_data, ctx->ui8SlotsInUse * ps_mem_size->weighted_prediction,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_weighted_prediction));

#ifdef LTREFHEADER
    ps_mem_size->lt_ref_header = ptg_align_KB((sizeof(MTX_HEADER_PARAMS)+63)&~63);
    ptg__alloc_init_buffer(ps_driver_data, ctx->ui8SlotsInUse * ps_mem_size->lt_ref_header,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_lt_ref_header));
#endif

    ps_mem_size->recon_pictures = ptg_align_KB((ptg_align_64(ui32_pic_width)*ptg_align_64(ui32_pic_height))*3/2);
    ptg__alloc_init_buffer(ps_driver_data, ctx->i32PicNodes * ps_mem_size->recon_pictures,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_recon_pictures));
    
    ps_mem_size->coded_buf = ps_mem_size->recon_pictures;
    ptg__alloc_init_buffer(ps_driver_data, ps_mem_size->coded_buf,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_coded_buf));

    ctx->ctx_mem_size.mb_ctrl_in_params = ptg_align_KB(sizeof(IMG_FIRST_STAGE_MB_PARAMS) * ui32_mb_width *  ui32_mb_height); 
    ptg__alloc_init_buffer(ps_driver_data, ctx->ui8SlotsInUse * ctx->ctx_mem_size.mb_ctrl_in_params,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_mb_ctrl_in_params));     

    ctx->ctx_mem_size.lowpower_params = ptg_align_KB(PTG_HEADER_SIZE);
    ptg__alloc_init_buffer(ps_driver_data, ps_mem_size->lowpower_params,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_lowpower_params));

    ctx->ctx_mem_size.lowpower_data = ptg_align_KB(0x10000);
    ptg__alloc_init_buffer(ps_driver_data, ps_mem_size->lowpower_data,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_lowpower_data));

    ptg__alloc_init_buffer(ps_driver_data, ps_mem_size->lowpower_data,
                                    psb_bt_cpu_vpu, &(ps_mem->bufs_lowpower_reg));

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: end\n", __FUNCTION__);

    return vaStatus;
}

static void ptg__free_context_buffer(context_ENC_p ctx, unsigned char is_JPEG, unsigned int stream_id)
{
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[stream_id]);    

    if (0 != is_JPEG) {
        psb_buffer_destroy(&(ctx->bufs_writeback));
        return;
    }
    psb_buffer_destroy(&(ps_mem->bufs_flat_gop));
    psb_buffer_destroy(&(ps_mem->bufs_hierar_gop));
    psb_buffer_destroy(&(ps_mem->bufs_mv_setting_btable));
    psb_buffer_destroy(&(ps_mem->bufs_mv_setting_hierar));
    psb_buffer_destroy(&(ps_mem->bufs_above_params));
    psb_buffer_destroy(&(ps_mem->bufs_colocated));
    psb_buffer_destroy(&(ps_mem->bufs_mv));
    psb_buffer_destroy(&(ctx->bufs_writeback));
    psb_buffer_destroy(&(ps_mem->bufs_slice_map));
    psb_buffer_destroy(&(ps_mem->bufs_weighted_prediction));
    psb_buffer_destroy(&(ps_mem->bufs_mb_ctrl_in_params));
    psb_buffer_destroy(&(ps_mem->bufs_coded_buf));
//    psb_buffer_destroy(&(ps_mem->bufs_ref_frames));
#ifdef LTREFHEADER
    psb_buffer_destroy(&(ps_mem->bufs_lt_ref_header));
#endif
    if (ctx->ui16UseCustomScalingLists)
	    psb_buffer_destroy(&(ps_mem->bufs_custom_quant));

    psb_buffer_destroy(&(ps_mem->bufs_lowpower_params));
    psb_buffer_destroy(&(ps_mem->bufs_lowpower_reg));
    psb_buffer_destroy(&(ps_mem->bufs_lowpower_data));

    return ;
}

unsigned int ptg__get_ipe_control(IMG_CODEC  eEncodingFormat)
{
    unsigned int RegVal = 0;

    RegVal = F_ENCODE(2, MVEA_CR_IPE_GRID_FINE_SEARCH) |
    F_ENCODE(0, MVEA_CR_IPE_GRID_SEARCH_SIZE) |
    F_ENCODE(1, MVEA_CR_IPE_Y_FINE_SEARCH);

    switch (eEncodingFormat) {
        case IMG_CODEC_H263_NO_RC:
        case IMG_CODEC_H263_VBR:
        case IMG_CODEC_H263_CBR:
            RegVal |= F_ENCODE(0, MVEA_CR_IPE_BLOCKSIZE) | F_ENCODE(0, MVEA_CR_IPE_ENCODING_FORMAT);
        break;
        case IMG_CODEC_MPEG4_NO_RC:
        case IMG_CODEC_MPEG4_VBR:
        case IMG_CODEC_MPEG4_CBR:
            RegVal |= F_ENCODE(1, MVEA_CR_IPE_BLOCKSIZE) | F_ENCODE(1, MVEA_CR_IPE_ENCODING_FORMAT);
        default:
        break;
        case IMG_CODEC_H264_NO_RC:
        case IMG_CODEC_H264_VBR:
        case IMG_CODEC_H264_CBR:
        case IMG_CODEC_H264_VCM:
            RegVal |= F_ENCODE(2, MVEA_CR_IPE_BLOCKSIZE) | F_ENCODE(2, MVEA_CR_IPE_ENCODING_FORMAT);
        break;
    }
    RegVal |= F_ENCODE(6, MVEA_CR_IPE_Y_CANDIDATE_NUM);
    return RegVal;
}

void ptg__setup_enc_profile_features(context_ENC_p ctx, IMG_UINT32 ui32EncProfile)
{
    IMG_ENCODE_FEATURES * pEncFeatures = &ctx->sEncFeatures;
    /* Set the default values first */
    pEncFeatures->bDisableBPicRef_0 = IMG_FALSE;
    pEncFeatures->bDisableBPicRef_1 = IMG_FALSE;

    pEncFeatures->bDisableInter8x8 = IMG_FALSE;
    pEncFeatures->bRestrictInter4x4 = IMG_FALSE;

    pEncFeatures->bDisableIntra4x4  = IMG_FALSE;
    pEncFeatures->bDisableIntra8x8  = IMG_FALSE;
    pEncFeatures->bDisableIntra16x16 = IMG_FALSE;

    pEncFeatures->bEnable8x16MVDetect   = IMG_TRUE;
    pEncFeatures->bEnable16x8MVDetect   = IMG_TRUE;
    pEncFeatures->bDisableBFrames       = IMG_FALSE;

    pEncFeatures->eMinBlkSz = BLK_SZ_DEFAULT;

    switch (ui32EncProfile) {
        case ENC_PROFILE_LOWCOMPLEXITY:
            pEncFeatures->bDisableInter8x8  = IMG_TRUE;
            pEncFeatures->bRestrictInter4x4 = IMG_TRUE;
            pEncFeatures->bDisableIntra4x4  = IMG_TRUE;
            pEncFeatures->bDisableIntra8x8  = IMG_TRUE;
            pEncFeatures->bRestrictInter4x4 = IMG_TRUE;
            pEncFeatures->eMinBlkSz         = BLK_SZ_16x16;
            pEncFeatures->bDisableBFrames   = IMG_TRUE;
            break;

        case ENC_PROFILE_HIGHCOMPLEXITY:
            pEncFeatures->bDisableBPicRef_0 = IMG_FALSE;
            pEncFeatures->bDisableBPicRef_1 = IMG_FALSE;

            pEncFeatures->bDisableInter8x8 = IMG_FALSE;
            pEncFeatures->bRestrictInter4x4 = IMG_FALSE;

            pEncFeatures->bDisableIntra4x4  = IMG_FALSE;
            pEncFeatures->bDisableIntra8x8  = IMG_FALSE;
            pEncFeatures->bDisableIntra16x16 = IMG_FALSE;

            pEncFeatures->bEnable8x16MVDetect   = IMG_TRUE;
            pEncFeatures->bEnable16x8MVDetect   = IMG_TRUE;
            break;
    }

    if (ctx->eStandard != IMG_STANDARD_H264) {
	pEncFeatures->bEnable8x16MVDetect = IMG_FALSE;
	pEncFeatures->bEnable16x8MVDetect = IMG_FALSE;
    }

    return;
}

VAStatus ptg__patch_hw_profile(context_ENC_p ctx)
{
    IMG_UINT32 ui32IPEControl = 0;
    IMG_UINT32 ui32PredCombControl = 0;
    IMG_ENCODE_FEATURES * psEncFeatures = &(ctx->sEncFeatures);

    // bDisableIntra4x4
    if (psEncFeatures->bDisableIntra4x4)
        ui32PredCombControl |= F_ENCODE(1, TOPAZHP_CR_INTRA4X4_DISABLE);

    //bDisableIntra8x8
    if (psEncFeatures->bDisableIntra8x8)
        ui32PredCombControl |= F_ENCODE(1, TOPAZHP_CR_INTRA8X8_DISABLE);

    //bDisableIntra16x16, check if atleast one of the other Intra mode is enabled
    if ((psEncFeatures->bDisableIntra16x16) &&
        (!(psEncFeatures->bDisableIntra8x8) || !(psEncFeatures->bDisableIntra4x4))) {
        ui32PredCombControl |= F_ENCODE(1, TOPAZHP_CR_INTRA16X16_DISABLE);
    }

    if (psEncFeatures->bRestrictInter4x4) {
//        ui32PredCombControl |= F_ENCODE(1, TOPAZHP_CR_INTER4X4_RESTRICT);
        ui32IPEControl |= F_ENCODE(1, TOPAZHP_CR_IPE_MV_NUMBER_RESTRICTION);
    }

    if (psEncFeatures->bDisableInter8x8)
        ui32PredCombControl |= F_ENCODE(1, TOPAZHP_CR_INTER8X8_DISABLE);

    if (psEncFeatures->bDisableBPicRef_1)
        ui32PredCombControl |= F_ENCODE(1, TOPAZHP_CR_B_PIC1_DISABLE);
    else if (psEncFeatures->bDisableBPicRef_0)
        ui32PredCombControl |= F_ENCODE(1, TOPAZHP_CR_B_PIC0_DISABLE);

    // save predictor combiner control in video encode parameter set
    ctx->ui32PredCombControl = ui32PredCombControl;

    // set blocksize
    ui32IPEControl |= F_ENCODE(psEncFeatures->eMinBlkSz, TOPAZHP_CR_IPE_BLOCKSIZE);

    if (psEncFeatures->bEnable8x16MVDetect)
        ui32IPEControl |= F_ENCODE(1, TOPAZHP_CR_IPE_8X16_ENABLE);

    if (psEncFeatures->bEnable16x8MVDetect)
        ui32IPEControl |= F_ENCODE(1, TOPAZHP_CR_IPE_16X8_ENABLE);

    if (psEncFeatures->bDisableBFrames)
        ctx->sRCParams.ui16BFrames = 0;

    //save IPE-control register
    ctx->ui32IPEControl = ui32IPEControl;

    return VA_STATUS_SUCCESS;
}

void ptg_DestroyContext(object_context_p obj_context, unsigned char is_JPEG)
{
    context_ENC_p ctx;
//    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;
    ctx = (context_ENC_p)obj_context->format_data;
    FRAME_ORDER_INFO *psFrameOrderInfo = &(ctx->sFrameOrderInfo);

    if (ptg_context_get_next_cmdbuf(ctx->obj_context))
        drv_debug_msg(VIDEO_DEBUG_ERROR, "get next cmdbuf fail\n");

    ptg_cmdbuf_insert_command_package(ctx->obj_context, 0,
        MTX_CMDID_SHUTDOWN, 0, 0);

    if (ptg_context_flush_cmdbuf(ctx->obj_context))
	drv_debug_msg(VIDEO_DEBUG_ERROR, "flush cmd package fail\n");

    if (psFrameOrderInfo->slot_consume_dpy_order != NULL)
        free(psFrameOrderInfo->slot_consume_dpy_order);
    if (psFrameOrderInfo->slot_consume_enc_order != NULL)
        free(psFrameOrderInfo->slot_consume_enc_order);

    ptg__free_context_buffer(ctx, is_JPEG, 0);
    if (ctx->bEnableMVC)
        ptg__free_context_buffer(ctx, is_JPEG, 1);

    free(obj_context->format_data);
    obj_context->format_data = NULL;
}

static VAStatus ptg__init_rc_params(context_ENC_p ctx, object_config_p obj_config)
{
    IMG_RC_PARAMS *psRCParams = &(ctx->sRCParams);
    unsigned int eRCmode = 0;
    memset(psRCParams, 0, sizeof(IMG_RC_PARAMS));
    IMG_INT32 i;

    //set RC mode
    for (i = 0; i < obj_config->attrib_count; i++) {
        if (obj_config->attrib_list[i].type == VAConfigAttribRateControl)
            break;
    }

    if (i >= obj_config->attrib_count) {
        eRCmode = VA_RC_NONE;
    } else {
        eRCmode = obj_config->attrib_list[i].value;
    }

	ctx->sRCParams.bRCEnable = IMG_TRUE;
    if (eRCmode == VA_RC_NONE) {
        ctx->sRCParams.bRCEnable = IMG_FALSE;
        ctx->sRCParams.eRCMode = IMG_RCMODE_NONE;
    } else if (eRCmode == VA_RC_CBR) {
        ctx->sRCParams.bDisableBitStuffing = IMG_FALSE;
        ctx->sRCParams.eRCMode = IMG_RCMODE_CBR;
    } else if (eRCmode == VA_RC_VBR) {
        ctx->sRCParams.eRCMode = IMG_RCMODE_VBR;
    } else if (eRCmode == VA_RC_VCM) {
        ctx->sRCParams.bRCEnable = IMG_RCMODE_VCM;
    } else {
        ctx->sRCParams.bRCEnable = IMG_FALSE;
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "not support this RT Format\n");
        return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
    }

    psRCParams->bScDetectDisable = IMG_FALSE;
    psRCParams->ui32SliceByteLimit = 0;
    psRCParams->ui32SliceMBLimit = 0;
    psRCParams->bIsH264Codec = (ctx->eStandard == IMG_STANDARD_H264) ? IMG_TRUE : IMG_FALSE;
    return VA_STATUS_SUCCESS;
}

/**************************************************************************************************
* Function:             IMG_C_GetEncoderCaps
* Description:  Get the capabilities of the encoder for the given codec
*
***************************************************************************************************/
//FIXME
static const IMG_UINT32 g_ui32PipesAvailable = 1;
static const IMG_UINT32 g_ui32CoreDes1 = 1;
static const IMG_UINT32 g_ui32CoreRev = 0x00030401;

static IMG_UINT32 ptg__get_num_pipes()
{
    return g_ui32PipesAvailable;
}

static IMG_UINT32 ptg__get_core_des1()
{
    return g_ui32CoreDes1;
}

static IMG_UINT32 ptg__get_core_rev()
{
    return g_ui32CoreRev;
}

static VAStatus ptg__get_encoder_caps(context_ENC_p ctx)
{
    IMG_ENC_CAPS *psCaps = &(ctx->sCapsParams);
    IMG_UINT16 ui16Height = ctx->ui16FrameHeight;
    IMG_UINT32 ui32NumCores = 0;
    IMG_UINT16 ui16MBRows = 0; //MB Rows in a GOB(slice);

    ctx->ui32CoreRev = ptg__get_core_rev();
    psCaps->ui32CoreFeatures = ptg__get_core_des1();
    
    /* get the actual number of cores */
    ui32NumCores = ptg__get_num_pipes();
    ctx->i32NumPipes = ui32NumCores;
        
    switch (ctx->eStandard) {
        case IMG_STANDARD_JPEG:
            psCaps->ui16MaxSlices = ui16Height / 8;
            psCaps->ui16MinSlices = 1;
            psCaps->ui16RecommendedSlices = ui32NumCores;
            break;
        case IMG_STANDARD_H264:
            psCaps->ui16MaxSlices = ui16Height / 16;
            psCaps->ui16MinSlices = 1;
            psCaps->ui16RecommendedSlices = ui32NumCores;
            break;
        case IMG_STANDARD_MPEG2:
            psCaps->ui16MaxSlices = 174; // Slice labelling dictates a maximum of 174 slices
            psCaps->ui16MinSlices = 1;
            psCaps->ui16RecommendedSlices = (ui16Height + 15) / 16;
            break;
        case IMG_STANDARD_H263:
            // if the original height of the pic is less than 400 , k is 1. refer standard.
            if (ui16Height <= 400) {
                ui16MBRows = 1;
            } else if (ui16Height < 800) { // if between 400 and 800 it's 2.            
                ui16MBRows = 2;
            } else {
                ui16MBRows = 4;
            }
            // before rounding is done for the height.
            // get the GOB's based on this MB Rows and not vice-versa.
            psCaps->ui16RecommendedSlices = (ui16Height + 15) >> (4 + (ui16MBRows >> 1));
            psCaps->ui16MaxSlices = psCaps->ui16MinSlices = psCaps->ui16RecommendedSlices;
            break;
        case IMG_STANDARD_MPEG4:
            psCaps->ui16MaxSlices = 1;
            psCaps->ui16MinSlices = 1;
            psCaps->ui16RecommendedSlices = 1;
            break;
        default:
            break;
    }
    return VA_STATUS_SUCCESS;
}

static VAStatus ptg_InitContext(context_ENC_p ctx)
{
    VAStatus vaStatus = 0;

    ptg__setup_enc_profile_features(ctx, ENC_PROFILE_DEFAULT);

    vaStatus = ptg__patch_hw_profile(ctx);
    if (vaStatus != VA_STATUS_SUCCESS) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: ptg__patch_hw_profile\n", __FUNCTION__);
    }
    
    
    /* Mostly sure about the following video parameters */
    //ctx->ui32HWProfile = pParams->ui32HWProfile;
    ctx->ui32FrameCount[0] = ctx->ui32FrameCount[1] = 0;
    /* Using Extended parameters */
    //carc params
    ctx->sCARCParams.bCARC             = 0;
    ctx->sCARCParams.i32CARCBaseline   = 0;
    ctx->sCARCParams.ui32CARCThreshold = TOPAZHP_DEFAULT_uCARCThreshold;
    ctx->sCARCParams.ui32CARCCutoff    = TOPAZHP_DEFAULT_uCARCCutoff;
    ctx->sCARCParams.ui32CARCNegRange  = TOPAZHP_DEFAULT_uCARCNegRange;
    ctx->sCARCParams.ui32CARCNegScale  = TOPAZHP_DEFAULT_uCARCNegScale;
    ctx->sCARCParams.ui32CARCPosRange  = TOPAZHP_DEFAULT_uCARCPosRange;
    ctx->sCARCParams.ui32CARCPosScale  = TOPAZHP_DEFAULT_uCARCPosScale;
    ctx->sCARCParams.ui32CARCShift     = TOPAZHP_DEFAULT_uCARCShift;

    ctx->bUseDefaultScalingList = IMG_FALSE;
    ctx->ui32CabacBinLimit = TOPAZHP_DEFAULT_uCABACBinLimit; //This parameter need not be exposed
    if (ctx->ui32CabacBinLimit == 0)
        ctx->ui32CabacBinFlex = 0;//This parameter need not be exposed
    else
        ctx->ui32CabacBinFlex = TOPAZHP_DEFAULT_uCABACBinFlex;//This parameter need not be exposed

    ctx->ui32FCode = 4;                     //This parameter need not be exposed
    ctx->iFineYSearchSize = 2;//This parameter need not be exposed
    ctx->ui32VopTimeResolution = 15;//This parameter need not be exposed
//    ctx->bEnabledDynamicBPic = IMG_FALSE;//Related to Rate Control,which is no longer needed.
    ctx->bH264IntraConstrained = IMG_FALSE;//This parameter need not be exposed
    ctx->bEnableInpCtrl     = IMG_FALSE;//This parameter need not be exposed
    ctx->bEnableAIR = 0;
    ctx->bEnableHostBias = (ctx->bEnableAIR != 0);//This parameter need not be exposed
    ctx->bEnableHostQP = IMG_FALSE; //This parameter need not be exposed
    ctx->ui8CodedSkippedIndex = 3;//This parameter need not be exposed
    ctx->ui8InterIntraIndex         = 3;//This parameter need not be exposed
    ctx->uMaxChunks = 0xA0;//This parameter need not be exposed
    ctx->uChunksPerMb = 0x40;//This parameter need not be exposed
    ctx->uPriorityChunks = (0xA0 - 0x60);//This parameter need not be exposed
    ctx->bWeightedPrediction = IMG_FALSE;//Weighted Prediction is not supported in TopazHP Version 3.0
    ctx->ui8VPWeightedImplicitBiPred = 0;//Weighted Prediction is not supported in TopazHP Version 3.0
    ctx->bSkipDuplicateVectors = IMG_FALSE;//By default false Newly Added
    ctx->bEnableCumulativeBiases = IMG_FALSE;//By default false Newly Added
    ctx->ui16UseCustomScalingLists = 0;//By default false Newly Added
    ctx->bPpsScaling = IMG_FALSE;//By default false Newly Added
    ctx->ui8MPEG2IntraDCPrecision = 0;//By default 0 Newly Added
    ctx->uMBspS = 0;//By default 0 Newly Added
    ctx->bMultiReferenceP = IMG_FALSE;//By default false Newly Added
    ctx->ui8RefSpacing=0;//By default 0 Newly Added
    ctx->bSpatialDirect = 0;//By default 0 Newly Added
    ctx->ui32DebugCRCs = 0;//By default false Newly Added
    ctx->bEnableMVC = IMG_FALSE;//By default false Newly Added
    ctx->ui16MVCViewIdx = (IMG_UINT16)(NON_MVC_VIEW);//Newly Added
    ctx->bSrcAllocInternally = IMG_FALSE;//By default false Newly Added
    ctx->bCodedAllocInternally = IMG_FALSE;//By default true Newly Added
    ctx->bHighLatency = IMG_TRUE;//Newly Added
    ctx->i32NumAIRMBs = -1;
    ctx->i32AIRThreshold = -1;
    ctx->i16AIRSkipCnt = -1;
    //Need to check on the following parameters
    ctx->ui8EnableSelStatsFlags  = IMG_FALSE;//Default Value ?? Extended Parameter ??
    ctx->bH2648x8Transform = IMG_FALSE;//Default Value ?? Extended Parameter or OMX_VIDEO_PARAM_AVCTYPE -> bDirect8x8Inference??
    //FIXME: Zhaohan, eStandard is always 0 here.
    ctx->bNoOffscreenMv = (ctx->eStandard == IMG_STANDARD_H263) ? IMG_TRUE : IMG_FALSE; //Default Value ?? Extended Parameter and bUseOffScreenMVUserSetting
    ctx->bNoSequenceHeaders = IMG_FALSE;
    ctx->bTopFieldFirst = IMG_TRUE;
    ctx->bOutputReconstructed = IMG_FALSE;
    ctx->sBiasTables.ui32FCode = ctx->ui32FCode;

    //Default fcode is 4
    if (!ctx->sBiasTables.ui32FCode)
	ctx->sBiasTables.ui32FCode = 4;

    ctx->uiCbrBufferTenths = TOPAZHP_DEFAULT_uiCbrBufferTenths;

    return VA_STATUS_SUCCESS;
}

VAStatus ptg_CreateContext(
    object_context_p obj_context,
    object_config_p obj_config,
    unsigned char is_JPEG)
{
    VAStatus vaStatus = 0;
    unsigned short ui16Width, ui16Height;
    context_ENC_p ctx;

    ui16Width = obj_context->picture_width;
    ui16Height = obj_context->picture_height;
    ctx = (context_ENC_p) calloc(1, sizeof(struct context_ENC_s));
    if (NULL == ctx) {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE;
        return vaStatus;
    }
    obj_context->format_data = (void*) ctx;
    ctx->obj_context = obj_context;

    if (is_JPEG == 0) {
        ctx->ui16Width = (unsigned short)(~0xf & (ui16Width + 0xf));
        ctx->ui16FrameHeight = (unsigned short)(~0xf & (ui16Height + 0xf));

        vaStatus = ptg_InitContext(ctx);
        if (vaStatus != VA_STATUS_SUCCESS) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "init Context params");
        } 

        vaStatus = ptg__init_rc_params(ctx, obj_config);
        if (vaStatus != VA_STATUS_SUCCESS) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "init rc params");
        } 
    } else {
        /*JPEG only require them are even*/
        ctx->ui16Width = (unsigned short)(~0x1 & (ui16Width + 0x1));
        ctx->ui16FrameHeight = (unsigned short)(~0x1 & (ui16Height + 0x1));
    }

    ctx->eFormat = IMG_CODEC_PL12;     // use default
    switch (obj_config->profile) {
        case VAProfileH264Baseline:
        case VAProfileH264ConstrainedBaseline:
            ctx->eStandard = IMG_STANDARD_H264;
            ctx->ui8ProfileIdc = 5;
            break;
        case VAProfileH264Main:
            ctx->eStandard = IMG_STANDARD_H264;
            ctx->ui8ProfileIdc = 6;
            break;
        case VAProfileH264High:
            ctx->eStandard = IMG_STANDARD_H264;
            ctx->ui8ProfileIdc = 7;
            break;
       case VAProfileH264StereoHigh:
            ctx->eStandard = IMG_STANDARD_H264;
            ctx->ui8ProfileIdc = 7;
            ctx->bEnableMVC = 1;
            ctx->ui16MVCViewIdx = 0;
            break;
        default:
            ctx->ui8ProfileIdc = 5;
        break;
    }
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: obj_config->profile = %dctx->eStandard = %d, ctx->bEnableMVC = %d\n", __FUNCTION__, obj_config->profile, ctx->eStandard, ctx->bEnableMVC);

    if (is_JPEG) {
	    vaStatus = ptg__alloc_context_buffer(ctx, is_JPEG, 0);
	    if (vaStatus != VA_STATUS_SUCCESS) {
		    drv_debug_msg(VIDEO_DEBUG_ERROR, "setup enc profile");
	    }
    }


    return vaStatus;
}

static void ptg_buffer_unmap(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: start \n", __FUNCTION__);
#endif
    psb_buffer_unmap(&(ps_mem->bufs_mtx_context));
    psb_buffer_unmap(&(ps_mem->bufs_seq_header)); 
    if (ctx->bEnableMVC)
        psb_buffer_unmap(&(ps_mem->bufs_sub_seq_header)); 
    if (ctx->bInsertHRDParams) {
        psb_buffer_unmap(&(ps_mem->bufs_sei_header));
    }
    psb_buffer_unmap(&(ps_mem->bufs_pic_template));
    psb_buffer_unmap(&(ps_mem->bufs_slice_template));
    psb_buffer_unmap(&(ctx->bufs_writeback));

    psb_buffer_unmap(&(ps_mem->bufs_above_params));
    psb_buffer_unmap(&(ps_mem->bufs_recon_pictures));
    psb_buffer_unmap(&(ps_mem->bufs_colocated));
    psb_buffer_unmap(&(ps_mem->bufs_mv));
    if (ctx->bEnableMVC)
        psb_buffer_unmap(&(ps_mem->bufs_interview_mv));
/*
    psb_buffer_unmap(&(ps_mem->bufs_bytes_coded));
    psb_buffer_unmap(&(ps_mem->bufs_src_phy_addr));
*/
    // WEIGHTED PREDICTION
    psb_buffer_unmap(&(ps_mem->bufs_weighted_prediction));
    psb_buffer_unmap(&(ps_mem->bufs_flat_gop));
    psb_buffer_unmap(&(ps_mem->bufs_hierar_gop));
    
#ifdef LTREFHEADER
    psb_buffer_unmap(&(ps_mem->bufs_lt_ref_header));
#endif

    if (ctx->ui16UseCustomScalingLists)
        psb_buffer_unmap(&(ps_mem->bufs_custom_quant));   
    psb_buffer_unmap(&(ps_mem->bufs_slice_map));
    
    /*  | MVSetingsB0 | MVSetingsB1 | ... | MVSetings Bn |  */
    psb_buffer_unmap(&(ps_mem->bufs_mv_setting_btable));
    psb_buffer_unmap(&(ps_mem->bufs_mv_setting_hierar));
/*
    psb_buffer_unmap(&(ps_mem->bufs_recon_buffer)); 
    psb_buffer_unmap(&(ps_mem->bufs_patch_recon_buffer));
    
    psb_buffer_unmap(&(ps_mem->bufs_first_pass_out_params));
#ifndef EXCLUDE_BEST_MP_DECISION_DATA
    psb_buffer_unmap(&(ps_mem->bufs_first_pass_out_best_multipass_param));
#endif
*/
    psb_buffer_unmap(&(ps_mem->bufs_mb_ctrl_in_params));
    psb_buffer_unmap(&(ps_mem->bufs_coded_buf));
//    psb_buffer_unmap(&(ps_mem->bufs_ref_frames));
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: end \n", __FUNCTION__);
#endif

    return ;
}

VAStatus ptg_BeginPicture(context_ENC_p ctx)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_ENC_frame_buf *ps_buf = &(ctx->ctx_frame_buf);
    ptg_cmdbuf_p cmdbuf;
    int ret;

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: start\n", __FUNCTION__);
    ctx->ui32StreamID = 0;

#ifdef _TOPAZHP_OLD_LIBVA_
    ps_buf->src_surface = ctx->obj_context->current_render_target;
#endif

    vaStatus = ptg__get_encoder_caps(ctx);
    if (vaStatus != VA_STATUS_SUCCESS) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: ptg__get_encoder_caps\n", __FUNCTION__);
    }

    /* clear frameskip flag to 0 */
    CLEAR_SURFACE_INFO_skipped_flag(ps_buf->src_surface->psb_surface);

    /* Initialise the command buffer */
    ret = ptg_context_get_next_cmdbuf(ctx->obj_context);
    if (ret) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "get next cmdbuf fail\n");
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        return vaStatus;
    }
    cmdbuf = ctx->obj_context->ptg_cmdbuf;

    //FIXME
    /* only map topaz param when necessary */
    cmdbuf->topaz_in_params_I_p = NULL;
    cmdbuf->topaz_in_params_P_p = NULL;
    ctx->obj_context->slice_count = 0;

    ptg_cmdbuf_buffer_ref(cmdbuf, &(ctx->obj_context->current_render_target->psb_surface->buf));
    
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: end\n", __FUNCTION__);

    return vaStatus;
}

static VAStatus ptg__provide_buffer_BFrames(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    IMG_RC_PARAMS * psRCParams = &(ctx->sRCParams);
    FRAME_ORDER_INFO *psFrameInfo = &(ctx->sFrameOrderInfo);
    static int frame_type;
    static int slot_num;
    static unsigned long long display_order;
    IMG_INT slot_buf = psRCParams->ui16BFrames + 2;
    IMG_UINT32 ui32FrameIdx = ctx->ui32FrameCount[ui32StreamIndex];
//    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;

#ifdef _PDUMP_BFRAME_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: frame count = %d\n", __FUNCTION__, (int)ui32FrameIdx);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: psRCParams->ui16BFrames = %d, psRCParams->ui32IntraFreq = %d, ctx->ui32IdrPeriod = %d\n", __FUNCTION__, (int)psRCParams->ui16BFrames, (int)psRCParams->ui32IntraFreq, ctx->ui32IdrPeriod);
#endif

//    ptg_send_codedbuf(ctx, (ui32FrameIdx&1));
    ptg_send_codedbuf(ctx, ui32StreamIndex);

    if (ui32StreamIndex == 0)
        getFrameDpyOrder(ui32FrameIdx,
            psRCParams->ui16BFrames, ctx->ui32IntraCnt, ctx->ui32IdrPeriod,
            psFrameInfo, &display_order, &frame_type, &slot_num);

#ifdef _PDUMP_BFRAME_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: slot_num = %d, display_order = %d\n", __FUNCTION__, slot_num, display_order);
#endif

    if (ui32FrameIdx < slot_buf) {
        if (ui32FrameIdx == 0)
            ptg_send_source_frame(ctx, 0, 0);
        else if (ui32FrameIdx == 1) {
            slot_num = 1;
            do {
                ptg_send_source_frame(ctx, slot_num, slot_num);
                ++slot_num;
            } while(slot_num < slot_buf);
        } else {
            slot_num = ui32FrameIdx - 1;
            ptg_send_source_frame(ctx, slot_num, slot_num);
        }
    } else {
        ptg_send_source_frame(ctx, slot_num , display_order);
    }
    return VA_STATUS_SUCCESS;
}


VAStatus ptg__provide_buffer_PFrames(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    IMG_UINT32 ui32FrameIdx = ctx->ui32FrameCount[ui32StreamIndex];
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: frame count = %d, SlotsInUse = %d, ui32FrameIdx = %d\n", __FUNCTION__, (int)ui32FrameIdx, ctx->ui8SlotsInUse, ui32FrameIdx);
#endif
    ptg_send_codedbuf(ctx, (ui32FrameIdx&1));
    ptg_send_source_frame(ctx, (ui32FrameIdx&1), ui32FrameIdx);

#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: end\n", __FUNCTION__);
#endif

    return VA_STATUS_SUCCESS;
}

static IMG_UINT16 H264_ROUNDING_OFFSETS[18][4] = {
    {  683, 683, 683, 683 },   /* 0  I-Slice - INTRA4  LUMA */
    {  683, 683, 683, 683 },   /* 1  P-Slice - INTRA4  LUMA */
    {  683, 683, 683, 683 },   /* 2  B-Slice - INTRA4  LUMA */

    {  683, 683, 683, 683 },   /* 3  I-Slice - INTRA8  LUMA */
    {  683, 683, 683, 683 },   /* 4  P-Slice - INTRA8  LUMA */
    {  683, 683, 683, 683 },   /* 5  B-Slice - INTRA8  LUMA */
    {  341, 341, 341, 341 },   /* 6  P-Slice - INTER8  LUMA */
    {  341, 341, 341, 341 },   /* 7  B-Slice - INTER8  LUMA */

    {  683, 683, 683, 000 },   /* 8  I-Slice - INTRA16 LUMA */
    {  683, 683, 683, 000 },   /* 9  P-Slice - INTRA16 LUMA */
    {  683, 683, 683, 000 },   /* 10 B-Slice - INTRA16 LUMA */
    {  341, 341, 341, 341 },   /* 11 P-Slice - INTER16 LUMA */
    {  341, 341, 341, 341 },   /* 12 B-Slice - INTER16 LUMA */

    {  683, 683, 683, 000 },   /* 13 I-Slice - INTRA16 CHROMA */
    {  683, 683, 683, 000 },   /* 14 P-Slice - INTRA16 CHROMA */
    {  683, 683, 683, 000 },   /* 15 B-Slice - INTRA16 CHROMA */
    {  341, 341, 341, 000 },   /* 16 P-Slice - INTER16 CHROMA */
    {  341, 341, 341, 000 } /* 17 B-Slice - INTER16 CHROMA */
};

static IMG_UINT16 ptg__create_gop_frame(
    IMG_UINT8 * pui8Level, IMG_BOOL bReference,
    IMG_UINT8 ui8Pos, IMG_UINT8 ui8Ref0Level,
    IMG_UINT8 ui8Ref1Level, IMG_FRAME_TYPE eFrameType)
{
    *pui8Level = ((ui8Ref0Level > ui8Ref1Level) ? ui8Ref0Level : ui8Ref1Level)  + 1;

    return F_ENCODE(bReference, GOP_REFERENCE) |
           F_ENCODE(ui8Pos, GOP_POS) |
           F_ENCODE(ui8Ref0Level, GOP_REF0) |
           F_ENCODE(ui8Ref1Level, GOP_REF1) |
           F_ENCODE(eFrameType, GOP_FRAMETYPE);
}

static void ptg__minigop_generate_flat(void* buffer_p, IMG_UINT32 ui32BFrameCount, IMG_UINT32 ui32RefSpacing, IMG_UINT8 aui8PicOnLevel[])
{
    /* B B B B P */
    IMG_UINT8 ui8EncodeOrderPos;
    IMG_UINT8 ui8Level;
    IMG_UINT16 * psGopStructure = (IMG_UINT16 *)buffer_p;

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);

    psGopStructure[0] = ptg__create_gop_frame(&ui8Level, IMG_TRUE, MAX_BFRAMES, ui32RefSpacing, 0, IMG_INTER_P);
    aui8PicOnLevel[ui8Level]++;

    for (ui8EncodeOrderPos = 1; ui8EncodeOrderPos < MAX_GOP_SIZE; ui8EncodeOrderPos++) {
        psGopStructure[ui8EncodeOrderPos] = ptg__create_gop_frame(&ui8Level, IMG_FALSE,
                                            ui8EncodeOrderPos - 1, ui32RefSpacing, ui32RefSpacing + 1, IMG_INTER_B);
        aui8PicOnLevel[ui8Level] = ui32BFrameCount;
    }
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s end\n", __FUNCTION__);

    for( ui8EncodeOrderPos = 0; ui8EncodeOrderPos < MAX_GOP_SIZE; ui8EncodeOrderPos++) {
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: psGopStructure = 0x%06x\n", __FUNCTION__, psGopStructure[ui8EncodeOrderPos]);
    }


    return ;
}

static void ptg__gop_split(IMG_UINT16 ** pasGopStructure, IMG_INT8 i8Ref0, IMG_INT8 i8Ref1,
                           IMG_UINT8 ui8Ref0Level, IMG_UINT8 ui8Ref1Level, IMG_UINT8 aui8PicOnLevel[])
{
    IMG_UINT8 ui8Distance = i8Ref1 - i8Ref0;
    IMG_UINT8 ui8Position = i8Ref0 + (ui8Distance >> 1);
    IMG_UINT8 ui8Level;

    if (ui8Distance == 1)
        return;

    /* mark middle as this level */

    (*pasGopStructure)++;
    **pasGopStructure = ptg__create_gop_frame(&ui8Level, ui8Distance >= 3, ui8Position, ui8Ref0Level, ui8Ref1Level, IMG_INTER_B);
    aui8PicOnLevel[ui8Level]++;

    if (ui8Distance >= 4)
        ptg__gop_split(pasGopStructure, i8Ref0, ui8Position, ui8Ref0Level, ui8Level, aui8PicOnLevel);

    if (ui8Distance >= 3)
        ptg__gop_split(pasGopStructure, ui8Position, i8Ref1, ui8Level, ui8Ref1Level, aui8PicOnLevel);
}

static void ptg_minigop_generate_hierarchical(void* buffer_p, IMG_UINT32 ui32BFrameCount,
        IMG_UINT32 ui32RefSpacing, IMG_UINT8 aui8PicOnLevel[])
{
    IMG_UINT8 ui8Level;
    IMG_UINT16 * psGopStructure = (IMG_UINT16 *)buffer_p;

    psGopStructure[0] = ptg__create_gop_frame(&ui8Level, IMG_TRUE, ui32BFrameCount, ui32RefSpacing, 0, IMG_INTER_P);
    aui8PicOnLevel[ui8Level]++;

    ptg__gop_split(&psGopStructure, -1, ui32BFrameCount, ui32RefSpacing, ui32RefSpacing + 1, aui8PicOnLevel);
}

static void ptg__generate_scale_tables(IMG_MTX_VIDEO_CONTEXT* psMtxEncCtx)
{
    psMtxEncCtx->ui32InterIntraScale[0] = 0x0004;  // Force intra by scaling its cost by 0
    psMtxEncCtx->ui32InterIntraScale[1] = 0x0103;  // favour intra by a factor 3
    psMtxEncCtx->ui32InterIntraScale[2] = 0x0102;  // favour intra by a factor 2
    psMtxEncCtx->ui32InterIntraScale[3] = 0x0101;  // no bias
    psMtxEncCtx->ui32InterIntraScale[4] = 0x0101;  // no bias
    psMtxEncCtx->ui32InterIntraScale[5] = 0x0201;  // favour inter by a factor 2
    psMtxEncCtx->ui32InterIntraScale[6] = 0x0301;  // favour inter by a factor 3
    psMtxEncCtx->ui32InterIntraScale[7] = 0x0400;  // Force inter by scaling it's cost by 0

    psMtxEncCtx->ui32SkippedCodedScale[0] = 0x0004;  // Force coded by scaling its cost by 0
    psMtxEncCtx->ui32SkippedCodedScale[1] = 0x0103;  // favour coded by a factor 3
    psMtxEncCtx->ui32SkippedCodedScale[2] = 0x0102;  // favour coded by a factor 2
    psMtxEncCtx->ui32SkippedCodedScale[3] = 0x0101;  // no bias
    psMtxEncCtx->ui32SkippedCodedScale[4] = 0x0101;  // no bias
    psMtxEncCtx->ui32SkippedCodedScale[5] = 0x0201;  // favour skipped by a factor 2
    psMtxEncCtx->ui32SkippedCodedScale[6] = 0x0301;  // favour skipped by a factor 3
    psMtxEncCtx->ui32SkippedCodedScale[7] = 0x0400;  // Force skipped by scaling it's cost by 0
    return ;
}

/*!
******************************************************************************
 @Function      ptg_update_driver_mv_scaling
 @details
        Setup the registers for scaling candidate motion vectors to take into account
        how far away (temporally) the reference pictures are
******************************************************************************/

static IMG_INT ptg__abs(IMG_INT a)
{
    if (a < 0)
        return -a;
    else
        return a;
}

static IMG_INT ptg__abs32(IMG_INT32 a)
{
    if (a < 0)
        return -a;
    else
        return a;
}
void ptg_update_driver_mv_scaling(
    IMG_UINT32 uFrameNum,
    IMG_UINT32 uRef0Num,
    IMG_UINT32 uRef1Num,
    IMG_UINT32 ui32PicFlags,
    IMG_BOOL   bSkipDuplicateVectors,
    IMG_UINT32 * pui32MVCalc_Below,
    IMG_UINT32 * pui32MVCalc_Colocated,
    IMG_UINT32 * pui32MVCalc_Config)
{
    IMG_UINT32 uMvCalcConfig = 0;
    IMG_UINT32 uMvCalcColocated = F_ENCODE(0x10, TOPAZHP_CR_TEMPORAL_BLEND);
    IMG_UINT32 uMvCalcBelow = 0;

    //If b picture calculate scaling factor for colocated motion vectors
    if (ui32PicFlags & ISINTERB_FLAGS) {
        IMG_INT tb, td, tx;
        IMG_INT iDistScale;

        //calculation taken from H264 spec
        tb = (uFrameNum * 2) - (uRef1Num * 2);
        td = (uRef0Num  * 2) - (uRef1Num * 2);
        tx = (16384 + ptg__abs(td / 2)) / td;
        iDistScale = (tb * tx + 32) >> 6;
        if (iDistScale > 1023) iDistScale = 1023;
        if (iDistScale < -1024) iDistScale = -1024;
        uMvCalcColocated |= F_ENCODE(iDistScale, TOPAZHP_CR_COL_DIST_SCALE_FACT);

        //We assume the below temporal mvs are from the latest reference frame
        //rather then the most recently encoded B frame (as Bs aren't reference)

        //Fwd temporal is same as colocated mv scale
        uMvCalcBelow     |= F_ENCODE(iDistScale, TOPAZHP_CR_PIC0_DIST_SCALE_FACTOR);

        //Bkwd temporal needs to be scaled by the recipricol amount in the other direction
        tb = (uFrameNum * 2) - (uRef0Num * 2);
        td = (uRef0Num  * 2) - (uRef1Num * 2);
        tx = (16384 + ptg__abs(td / 2)) / td;
        iDistScale = (tb * tx + 32) >> 6;
        if (iDistScale > 1023) iDistScale = 1023;
        if (iDistScale < -1024) iDistScale = -1024;
        uMvCalcBelow |= F_ENCODE(iDistScale, TOPAZHP_CR_PIC1_DIST_SCALE_FACTOR);
    } else {
        //Don't scale the temporal below mvs
        uMvCalcBelow |= F_ENCODE(1 << 8, TOPAZHP_CR_PIC0_DIST_SCALE_FACTOR);

        if (uRef0Num != uRef1Num) {
            IMG_INT iRef0Dist, iRef1Dist;
            IMG_INT iScale;

            //Distance to second reference picture may be different when
            //using multiple reference frames on P. Scale based on difference
            //in temporal distance to ref pic 1 compared to distance to ref pic 0
            iRef0Dist = (uFrameNum - uRef0Num);
            iRef1Dist = (uFrameNum - uRef1Num);
            iScale    = (iRef1Dist << 8) / iRef0Dist;

            if (iScale > 1023) iScale = 1023;
            if (iScale < -1024) iScale = -1024;

            uMvCalcBelow |= F_ENCODE(iScale, TOPAZHP_CR_PIC1_DIST_SCALE_FACTOR);
        } else
            uMvCalcBelow |= F_ENCODE(1 << 8, TOPAZHP_CR_PIC1_DIST_SCALE_FACTOR);
    }

    if (uFrameNum > 0) {
        IMG_INT ref0_distance, ref1_distance;
        IMG_INT jitter0, jitter1;

        ref0_distance = ptg__abs32((IMG_INT32)uFrameNum - (IMG_INT32)uRef0Num);
        ref1_distance = ptg__abs32((IMG_INT32)uFrameNum - (IMG_INT32)uRef1Num);

        if (!(ui32PicFlags & ISINTERB_FLAGS)) {
            jitter0 = ref0_distance * 1;
            jitter1 = jitter0 > 1 ? 1 : 2;
        } else {
            jitter0 = ref1_distance * 1;
            jitter1 = ref0_distance * 1;
        }

//Hardware can only cope with 1 - 4 jitter factors
        jitter0 = (jitter0 > 4) ? 4 : (jitter0 < 1) ? 1 : jitter0;
        jitter1 = (jitter1 > 4) ? 4 : (jitter1 < 1) ? 1 : jitter1;

        //Hardware can only cope with 1 - 4 jitter factors
        assert(jitter0 > 0 && jitter0 <= 4 && jitter1 > 0 && jitter1 <= 4);

        uMvCalcConfig |= F_ENCODE(jitter0 - 1, TOPAZHP_CR_MVCALC_IPE0_JITTER_FACTOR) |
                         F_ENCODE(jitter1 - 1, TOPAZHP_CR_MVCALC_IPE1_JITTER_FACTOR);
    }

    uMvCalcConfig |= F_ENCODE(1, TOPAZHP_CR_MVCALC_DUP_VEC_MARGIN);
    uMvCalcConfig |= F_ENCODE(7, TOPAZHP_CR_MVCALC_GRID_MB_X_STEP);
    uMvCalcConfig |= F_ENCODE(13, TOPAZHP_CR_MVCALC_GRID_MB_Y_STEP);
    uMvCalcConfig |= F_ENCODE(3, TOPAZHP_CR_MVCALC_GRID_SUB_STEP);
    uMvCalcConfig |= F_ENCODE(1, TOPAZHP_CR_MVCALC_GRID_DISABLE);

    if (bSkipDuplicateVectors)
        uMvCalcConfig |= F_ENCODE(1, TOPAZHP_CR_MVCALC_NO_PSEUDO_DUPLICATES);

    * pui32MVCalc_Below =   uMvCalcBelow;
    * pui32MVCalc_Colocated = uMvCalcColocated;
    * pui32MVCalc_Config = uMvCalcConfig;
}

static void ptg__prepare_mv_estimates(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    context_ENC_mem* ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
    IMG_MTX_VIDEO_CONTEXT* psMtxEncCtx = NULL;
    IMG_UINT32 ui32Distance;

#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_map(&(ps_mem->bufs_mtx_context), &(ps_mem->bufs_mtx_context.virtual_addr));
#endif
    psMtxEncCtx = (IMG_MTX_VIDEO_CONTEXT*)(ps_mem->bufs_mtx_context.virtual_addr);

    /* IDR */
    psMtxEncCtx->sMVSettingsIdr.ui32MVCalc_Config = DEFAULT_MVCALC_CONFIG;  // default based on TRM
    psMtxEncCtx->sMVSettingsIdr.ui32MVCalc_Colocated = 0x00100100;// default based on TRM
    psMtxEncCtx->sMVSettingsIdr.ui32MVCalc_Below = 0x01000100;      // default based on TRM

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);

    ptg_update_driver_mv_scaling(
        0, 0, 0, 0, IMG_FALSE, //psMtxEncCtx->bSkipDuplicateVectors, //By default false Newly Added
        &psMtxEncCtx->sMVSettingsIdr.ui32MVCalc_Below,
        &psMtxEncCtx->sMVSettingsIdr.ui32MVCalc_Colocated,
        &psMtxEncCtx->sMVSettingsIdr.ui32MVCalc_Config);

    /* NonB (I or P) */
    for (ui32Distance = 1; ui32Distance <= MAX_BFRAMES + 1; ui32Distance++) {
        psMtxEncCtx->sMVSettingsNonB[ui32Distance - 1].ui32MVCalc_Config = DEFAULT_MVCALC_CONFIG;       // default based on TRM
        psMtxEncCtx->sMVSettingsNonB[ui32Distance - 1].ui32MVCalc_Colocated = 0x00100100;// default based on TRM
        psMtxEncCtx->sMVSettingsNonB[ui32Distance - 1].ui32MVCalc_Below = 0x01000100;   // default based on TRM


        ptg_update_driver_mv_scaling(ui32Distance, 0, 0, 0, IMG_FALSE, //psMtxEncCtx->bSkipDuplicateVectors,
                                     &psMtxEncCtx->sMVSettingsNonB[ui32Distance - 1].ui32MVCalc_Below,
                                     &psMtxEncCtx->sMVSettingsNonB[ui32Distance - 1].ui32MVCalc_Colocated,
                                     &psMtxEncCtx->sMVSettingsNonB[ui32Distance - 1].ui32MVCalc_Config);
    }

    {
        IMG_UINT32 ui32DistanceB;
        IMG_UINT32 ui32Position;
        context_ENC_mem *ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
        IMG_MV_SETTINGS *pHostMVSettingsHierarchical = NULL;
        IMG_MV_SETTINGS *pMvElement = NULL;
        IMG_MV_SETTINGS *pHostMVSettingsBTable = NULL;

#ifdef _TOPAZHP_VIR_ADDR_
        psb_buffer_map(&(ps_mem->bufs_mv_setting_btable), &(ps_mem->bufs_mv_setting_btable.virtual_addr));
#endif
        pHostMVSettingsBTable = (IMG_MV_SETTINGS *)(ps_mem->bufs_mv_setting_btable.virtual_addr);

        for (ui32DistanceB = 0; ui32DistanceB < MAX_BFRAMES; ui32DistanceB++) {
            for (ui32Position = 1; ui32Position <= ui32DistanceB + 1; ui32Position++) {
                pMvElement = (IMG_MV_SETTINGS * ) ((IMG_UINT8 *) pHostMVSettingsBTable + MV_OFFSET_IN_TABLE(ui32DistanceB, ui32Position - 1));
                pMvElement->ui32MVCalc_Config= (DEFAULT_MVCALC_CONFIG|MASK_TOPAZHP_CR_MVCALC_GRID_DISABLE);    // default based on TRM
                pMvElement->ui32MVCalc_Colocated=0x00100100;// default based on TRM
                pMvElement->ui32MVCalc_Below=0x01000100;	// default based on TRM

                ptg_update_driver_mv_scaling(
                    ui32Position, ui32DistanceB + 2, 0, ISINTERB_FLAGS, IMG_FALSE,
                    &pMvElement->ui32MVCalc_Below,
                    &pMvElement->ui32MVCalc_Colocated,
                    &pMvElement->ui32MVCalc_Config);
            }
        }

        if (ctx->b_is_mv_setting_hierar){
            pHostMVSettingsHierarchical = (IMG_MV_SETTINGS *)(ps_mem->bufs_mv_setting_hierar.virtual_addr);

            for (ui32DistanceB = 0; ui32DistanceB < MAX_BFRAMES; ui32DistanceB++) {
                pMvElement = (IMG_MV_SETTINGS * ) ((IMG_UINT8 *)pHostMVSettingsBTable + MV_OFFSET_IN_TABLE(ui32DistanceB, ui32DistanceB >> 1));
                //memcpy(pHostMVSettingsHierarchical + ui32DistanceB, , sizeof(IMG_MV_SETTINGS));
                pHostMVSettingsHierarchical[ui32DistanceB].ui32MVCalc_Config    = pMvElement->ui32MVCalc_Config;
                pHostMVSettingsHierarchical[ui32DistanceB].ui32MVCalc_Colocated = pMvElement->ui32MVCalc_Colocated;
                pHostMVSettingsHierarchical[ui32DistanceB].ui32MVCalc_Below     = pMvElement->ui32MVCalc_Below;
            }
        }
#ifdef _TOPAZHP_VIR_ADDR_
        psb_buffer_unmap(&(ps_mem->bufs_mv_setting_btable));
#endif
    }

#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_unmap(&(ps_mem->bufs_mtx_context));
#endif

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s end\n", __FUNCTION__);

    return ;
}

static void ptg__adjust_picflags(
    context_ENC_p ctx,
    IMG_RC_PARAMS * psRCParams,
    IMG_BOOL bFirstPic,
    IMG_UINT32 * pui32Flags)
{
    IMG_UINT32 ui32Flags;
    PIC_PARAMS * psPicParams = &ctx->sPicParams;
    ui32Flags = psPicParams->ui32Flags;

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);

    if (!psRCParams->bRCEnable || (!bFirstPic))
        ui32Flags = 0;

    switch (ctx->eStandard) {
    case IMG_STANDARD_NONE:
        break;
    case IMG_STANDARD_H264:
        break;
    case IMG_STANDARD_H263:
        ui32Flags |= ISH263_FLAGS;
        break;
    case IMG_STANDARD_MPEG4:
        ui32Flags |= ISMPEG4_FLAGS;
        break;
    case IMG_STANDARD_MPEG2:
        ui32Flags |= ISMPEG2_FLAGS;
        break;
    default:
        break;
    }
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s end\n", __FUNCTION__);

    * pui32Flags = ui32Flags;
}

#define gbLowLatency 0

static void ptg__setup_rcdata(context_ENC_p ctx)
{
    IMG_RC_PARAMS *psRCParams = &(ctx->sRCParams);
    PIC_PARAMS    *psPicParams = &(ctx->sPicParams);
    IN_RC_PARAMS  *psInParams = &(ctx->sPicParams.sInParams);
    IMG_INT32  i32FrameRate, i32TmpQp;
    double     L1, L2, L3, L4, L5, L6, flBpp;
    IMG_INT32  i32BufferSizeInFrames;
    IMG_UINT32 ui32PicSize = (IMG_UINT32)(ctx->ui16PictureHeight) * (IMG_UINT32)(ctx->ui16Width);
    IMG_UINT32 ui32QualPicsize = ui32PicSize >> 8;
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);
#endif

    // If Bit Rate and Basic Units are not specified then set to default values.
    if (psRCParams->ui32BitsPerSecond == 0 && !ctx->bEnableMVC) {
        psRCParams->ui32BitsPerSecond = 640000;         // kbps
    }
    if (!psRCParams->ui32BUSize)    {
        psRCParams->ui32BUSize = ui32QualPicsize;               // BU = 1 Frame
    }

    ctx->ui32KickSize = psRCParams->ui32BUSize;

    if (!psRCParams->ui32FrameRate) {
        psRCParams->ui32FrameRate = 30;         // fps
    }

    // Calculate Bits Per Pixel
    i32FrameRate    = (ctx->ui16Width <= 176) ? 30 : psRCParams->ui32FrameRate;
    flBpp = 1.0 * psRCParams->ui32BitsPerSecond / (i32FrameRate * ui32PicSize);
    psInParams->ui8SeInitQP     = psRCParams->ui32InitialQp;
    psInParams->ui8MBPerRow     = (ctx->ui16Width >> 4);
    psInParams->ui16MBPerBU     = psRCParams->ui32BUSize;
    psInParams->ui16MBPerFrm    = ui32QualPicsize;
    psInParams->ui16BUPerFrm    = (psInParams->ui16MBPerFrm) / psRCParams->ui32BUSize;
    psInParams->ui16IntraPeriod = psRCParams->ui32IntraFreq;
    psInParams->ui16BFrames     = psRCParams->ui16BFrames;
    psInParams->i32BitRate      = psRCParams->ui32BitsPerSecond;

    //FIXME: Zhaohan, this should be set by testsuite?
    psRCParams->ui32TransferBitsPerSecond = psInParams->i32BitRate;

    psInParams->bFrmSkipDisable = psRCParams->bDisableFrameSkipping;

    psInParams->i32BitsPerFrm   = (psRCParams->ui32BitsPerSecond + psRCParams->ui32FrameRate / 2) / psRCParams->ui32FrameRate;
    psInParams->i32TransferRate = (psRCParams->ui32TransferBitsPerSecond + psRCParams->ui32FrameRate/2) / psRCParams->ui32FrameRate;
    psInParams->i32BitsPerGOP   = (psRCParams->ui32BitsPerSecond / psRCParams->ui32FrameRate) * psRCParams->ui32IntraFreq;
    psInParams->i32BitsPerBU    = psInParams->i32BitsPerFrm / (4 * psInParams->ui16BUPerFrm);
    psInParams->i32BitsPerMB    = psInParams->i32BitsPerBU / psRCParams->ui32BUSize;

    psInParams->ui16AvQPVal     = psRCParams->ui32InitialQp;
    psInParams->ui16MyInitQP    = psRCParams->ui32InitialQp;
#ifdef _PDUMP_FUNC_
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s psRCParams->ui32BUSize = %d, i32FrameRate = %d, ui32PicSize = %d, psInParams->ui16BUPerFrm = %d\n", __FUNCTION__, psRCParams->ui32BUSize, i32FrameRate, ui32PicSize, psInParams->ui16BUPerFrm);
#endif

    psInParams->bHierarchicalMode = psRCParams->b16Hierarchical;
    if (psInParams->i32BitsPerFrm) {
        i32BufferSizeInFrames = (psRCParams->ui32BufferSize + (psInParams->i32BitsPerFrm / 2)) / psInParams->i32BitsPerFrm;
    } else {
        IMG_ASSERT(ctx->bEnableMVC && "Can happen only in MVC mode");
        /* Asigning more or less `normal` value. To be overriden by MVC RC module */
        i32BufferSizeInFrames = 30;
    }
#ifdef _PDUMP_FUNC_
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ctx->eStandard = %d\n", __FUNCTION__, ctx->eStandard);
#endif
    // select thresholds and initial Qps etc that are codec dependent
    switch (ctx->eStandard) {
    case IMG_STANDARD_NONE:
        break;

    case IMG_STANDARD_H264:
        L1 = 0.1;
        L2 = 0.15;
        L3 = 0.2;
        psInParams->ui8MaxQPVal = 51;
        ctx->ui32KickSize = psInParams->ui16MBPerBU;

        // Setup MAX and MIN Quant Values
        if (psRCParams->iMinQP == 0)    {
            if (flBpp >= 0.50)
                i32TmpQp = 4;
            else if (flBpp > 0.133)
                i32TmpQp = (IMG_INT32)(22 - (40 * flBpp));
            else
                i32TmpQp = (IMG_INT32)(30 - (100 * flBpp));

            /* Adjust minQp up for small buffer size and down for large buffer size */
            if (i32BufferSizeInFrames < 20) {
                i32TmpQp += 2;
            }
            if (i32BufferSizeInFrames > 40) {
                if (i32TmpQp >= 1)
                    i32TmpQp -= 1;
            }
            /* for HD content allow a lower minQp as bitrate is more easily controlled in this case */
            if (psInParams->ui16MBPerFrm > 2000) {
                if (i32TmpQp >= 2)
                    i32TmpQp -= 2;
            }

        } else
            i32TmpQp = psRCParams->iMinQP;

        psInParams->ui8MinQPVal = (i32TmpQp < 4) ? 4 : i32TmpQp;
        // Calculate Initial QP if it has not been specified
        i32TmpQp = psInParams->ui8SeInitQP;

        if (psInParams->ui8SeInitQP == 0) {
            L1 = 0.050568;
            L2 = 0.202272;
            L3 = 0.40454321;
            L4 = 0.80908642;
            L5 = 1.011358025;
            if (flBpp < L1)
                i32TmpQp = (IMG_INT32)(45 - 78.10 * flBpp);
            else if (flBpp >= L1 && flBpp < L2)
                i32TmpQp = (IMG_INT32)(44 - 72.51 * flBpp);
            else if (flBpp >= L2 && flBpp < L3)
                i32TmpQp = (IMG_INT32)(34 - 24.72 * flBpp);
            else if (flBpp >= L3 && flBpp < L4)
                i32TmpQp = (IMG_INT32)(32 - 19.78 * flBpp);
            else if (flBpp >= L4 && flBpp < L5)
                i32TmpQp = (IMG_INT32)(25 - 9.89 * flBpp);
            else if (flBpp >= L5)
                i32TmpQp = (IMG_INT32)(18 - 4.95 * flBpp);
            /* Adjust ui8SeInitQP up for small buffer size or small fps */
            /* Adjust ui8SeInitQP up for small gop size */
            if ((i32BufferSizeInFrames < 20) || (psRCParams->ui32IntraFreq < 20)) {
                i32TmpQp += 2;
            }
            /* start on a lower initial Qp for HD content as the coding is more efficient */
            if (psInParams->ui16MBPerFrm > 2000) {
                i32TmpQp -= 2;
            }
        }

        if (gbLowLatency) {
            if (i32TmpQp > 40)
                i32TmpQp += 5;
            else if (i32TmpQp > 35)
                i32TmpQp += 7;
            else if (i32TmpQp > 32)
                i32TmpQp += 9;
            else
                i32TmpQp += 12;
        }
        if (i32TmpQp > 49) {
            i32TmpQp = 49;
        }
        if (i32TmpQp < psInParams->ui8MinQPVal) {
            i32TmpQp = psInParams->ui8MinQPVal;
        }
        psInParams->ui8SeInitQP = i32TmpQp;

        if (flBpp <= 0.3)
            psPicParams->ui32Flags |= ISRC_I16BIAS;
        break;
    case IMG_STANDARD_MPEG4:
    case IMG_STANDARD_MPEG2:
    case IMG_STANDARD_H263:
        psInParams->ui8MaxQPVal  = 31;
        if (ctx->ui16Width == 176) {
            L1 = 0.042;
            L2 = 0.084;
            L3 = 0.126;
            L4 = 0.168;
            L5 = 0.336;
            L6 = 0.505;
        } else if (ctx->ui16Width == 352) {
            L1 = 0.064;
            L2 = 0.084;
            L3 = 0.106;
            L4 = 0.126;
            L5 = 0.168;
            L6 = 0.210;
        } else {
            L1 = 0.050;
            L2 = 0.0760;
            L3 = 0.096;
            L4 = 0.145;
            L5 = 0.193;
            L6 = 0.289;
        }

        if (psInParams->ui8SeInitQP == 0) {
            if (flBpp < L1)
                psInParams->ui8SeInitQP = 31;
            else if (flBpp >= L1 && flBpp < L2)
                psInParams->ui8SeInitQP = 26;
            else if (flBpp >= L2 && flBpp < L3)
                psInParams->ui8SeInitQP = 22;
            else if (flBpp >= L3 && flBpp < L4)
                psInParams->ui8SeInitQP = 18;
            else if (flBpp >= L4 && flBpp < L5)
                psInParams->ui8SeInitQP = 14;
            else if (flBpp >= L5 && flBpp < L6)
                psInParams->ui8SeInitQP = 10;
            else
                psInParams->ui8SeInitQP = 8;

            /* Adjust ui8SeInitQP up for small buffer size or small fps */
            /* Adjust ui8SeInitQP up for small gop size */
            if ((i32BufferSizeInFrames < 20) || (psRCParams->ui32IntraFreq < 20)) {
                psInParams->ui8SeInitQP += 2;
            }

            if (psInParams->ui8SeInitQP > psInParams->ui8MaxQPVal) {
                psInParams->ui8SeInitQP = psInParams->ui8MaxQPVal;
            }
            psInParams->ui16AvQPVal =  psInParams->ui8SeInitQP;
        }

        psInParams->ui8MinQPVal = 2;

        /* Adjust minQp up for small buffer size and down for large buffer size */
        if (i32BufferSizeInFrames < 20) {
            psInParams->ui8MinQPVal += 1;
        }

        break;
    default:
        /* the NO RC cases will fall here */
        break;
    }
#ifdef _PDUMP_FUNC_
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ctx->sRCParams.eRCMode = %d\n", __FUNCTION__, ctx->sRCParams.eRCMode);
#endif
    if (ctx->sRCParams.eRCMode == IMG_RCMODE_VBR) {
        psInParams->ui16MBPerBU = psInParams->ui16MBPerFrm;
        psInParams->ui16BUPerFrm        = 1;

        // Initialize the parameters of fluid flow traffic model.
        psInParams->i32BufferSize = psRCParams->ui32BufferSize;

        // These scale factor are used only for rate control to avoid overflow
        // in fixed-point calculation these scale factors are decided by bit rate
        if (psRCParams->ui32BitsPerSecond < 640000) {
            psInParams->ui8ScaleFactor  = 2;        // related to complexity
        } else if (psRCParams->ui32BitsPerSecond < 2000000)     {// 2 Mbits
            psInParams->ui8ScaleFactor  = 4;
        } else if (psRCParams->ui32BitsPerSecond < 8000000) {// 8 Mbits
            psInParams->ui8ScaleFactor  = 6;
        } else
            psInParams->ui8ScaleFactor  = 8;
    } else {
        // Set up Input Parameters that are mode dependent
        switch (ctx->eStandard) {
        case IMG_STANDARD_NONE:
        case IMG_STANDARD_JPEG:
            break;
        case IMG_STANDARD_H264:
            // ------------------- H264 CBR RC ------------------- //
            // Initialize the parameters of fluid flow traffic model.
            psInParams->i32BufferSize = psRCParams->ui32BufferSize;

            // HRD consideration - These values are used by H.264 reference code.
            if (psRCParams->ui32BitsPerSecond < 1000000)         // 1 Mbits/s
                psInParams->ui8ScaleFactor = 0;
            else if (psRCParams->ui32BitsPerSecond < 2000000)    // 2 Mbits/s
                psInParams->ui8ScaleFactor = 1;
            else if (psRCParams->ui32BitsPerSecond < 4000000)    // 4 Mbits/s
                psInParams->ui8ScaleFactor = 2;
            else if (psRCParams->ui32BitsPerSecond < 8000000)    // 8 Mbits/s
                psInParams->ui8ScaleFactor = 3;
            else
                psInParams->ui8ScaleFactor = 4;

            if (ctx->sRCParams.eRCMode == IMG_RCMODE_VCM) {
                if (ctx->ui16FrameHeight >= 480) {
                    psInParams->ui8VCMBitrateMargin = 126;
                } else {
                    psInParams->ui8VCMBitrateMargin = 115;
                }
                if (i32BufferSizeInFrames < 15) {
                    // when we have a very small window size we reduce the target further to avoid too much skipping
                    psInParams->ui8VCMBitrateMargin -= 5;
                }
                psInParams->i32ForceSkipMargin = 500; // start skipping MBs when within 500 bits of slice or frame limit
                psInParams->i32BufferSize = i32BufferSizeInFrames;
            }
            break;

        case IMG_STANDARD_MPEG4:
        case IMG_STANDARD_MPEG2:
        case IMG_STANDARD_H263:
            flBpp  = 256 * (psRCParams->ui32BitsPerSecond / ctx->ui16Width);
            flBpp /= (ctx->ui16FrameHeight * psRCParams->ui32FrameRate);

            if ((psInParams->ui16MBPerFrm > 1024 && flBpp < 16) ||
                (psInParams->ui16MBPerFrm <= 1024 && flBpp < 24))
                psInParams->ui8HalfFrameRate = 1;
            else
                psInParams->ui8HalfFrameRate = 0;

            if (psInParams->ui8HalfFrameRate >= 1)  {
                psInParams->ui8SeInitQP = 31;
                psInParams->ui16AvQPVal = 31;
                psInParams->ui16MyInitQP = 31;
            }

            psInParams->i32BufferSize = psRCParams->ui32BufferSize;
            break;
        }
    }

    if (psRCParams->bScDetectDisable)
        psPicParams->ui32Flags  |= ISSCENE_DISABLED;

    psInParams->bRCIsH264VBR =
        (ctx->eStandard == IMG_STANDARD_H264 && ctx->sRCParams.eRCMode == IMG_RCMODE_VBR);

    psInParams->ui16MyInitQP        = psInParams->ui8SeInitQP;

    /* Low Delay Buffer Constraints */
    if (gbLowLatency) {
        psRCParams->i32InitialLevel     = (IMG_INT32) 1.0 * psRCParams->ui32BitsPerSecond / psRCParams->ui32FrameRate;
        psRCParams->ui32BufferSize      = 3 * (psRCParams->ui32BitsPerSecond / psRCParams->ui32FrameRate);
        psRCParams->i32InitialDelay     = psRCParams->ui32BufferSize - psRCParams->i32InitialLevel;
    }

    psInParams->i32InitialDelay     = psRCParams->i32InitialDelay;
    psInParams->i32InitialLevel     = psRCParams->i32InitialLevel;
    psRCParams->ui32InitialQp       = psInParams->ui8SeInitQP;
#ifdef _PDUMP_FUNC_
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s psInParams->i32BufferSize = %d, psInParams->i32InitialLevel = %d\n", __FUNCTION__, psInParams->i32BufferSize, psInParams->i32InitialLevel);
#endif
    /* The rate control uses this value to adjust the reaction rate to larger than expected frames */
    if (psInParams->i32BitsPerFrm) {
        psInParams->ui32RCScaleFactor =
            (psInParams->i32BitsPerGOP * 256) /
            (psInParams->i32BufferSize - psInParams->i32InitialLevel);
    } else {
        IMG_ASSERT(psInParams->i32BitsPerGOP == 0);
        IMG_ASSERT(psInParams->i32BufferSize == 0);
        IMG_ASSERT(psInParams->i32InitialLevel == 0);
        psInParams->ui32RCScaleFactor = 0;
    }
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s end\n", __FUNCTION__);
#endif

    return ;
}

static void ptg__save_slice_params_template(
    context_ENC_p ctx,
    IMG_UINT32  ui32SliceBufIdx,
    IMG_UINT32  ui32SliceType,
    IMG_UINT32  ui32IPEControl,
    IMG_UINT32  ui32Flags,
    IMG_UINT32  ui32SliceConfig,
    IMG_UINT32  ui32SeqConfig,
    IMG_UINT32  ui32StreamIndex
)
{
    IMG_FRAME_TEMPLATE_TYPE eSliceType = (IMG_FRAME_TEMPLATE_TYPE)ui32SliceType;
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
    SLICE_PARAMS *slice_temp_p = NULL;
#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_map(&(ps_mem->bufs_slice_template), &(ps_mem->bufs_slice_template.virtual_addr));
#endif
    slice_temp_p = (SLICE_PARAMS*)(ps_mem->bufs_slice_template.virtual_addr + (ctx->ctx_mem_size.slice_template * ui32SliceBufIdx));

    slice_temp_p->eTemplateType = eSliceType;
    slice_temp_p->ui32Flags = ui32Flags;
    slice_temp_p->ui32IPEControl = ui32IPEControl;
    slice_temp_p->ui32SliceConfig = ui32SliceConfig;
    slice_temp_p->ui32SeqConfig = ui32SeqConfig;
#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_unmap(&(ps_mem->bufs_slice_template));
#endif
    return ;
}


/*****************************************************************************
 * Function Name        :       PrepareEncodeSliceParams
 *
 ****************************************************************************/
void ptg__trace_seqconfig(
    IMG_BOOL bIsBPicture,
    IMG_BOOL bFieldMode,
    IMG_UINT8  ui8SwapChromas,
    IMG_BOOL32 ui32FrameStoreFormat,
    IMG_UINT8  uiDeblockIDC)
{
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(0, TOPAZHP_CR_TEMPORAL_PIC0_BELOW_IN_VALID)                      );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(0, TOPAZHP_CR_TEMPORAL_PIC1_BELOW_IN_VALID)                    );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(0, TOPAZHP_CR_ABOVE_OUT_OF_SLICE_VALID)                        );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(1, TOPAZHP_CR_WRITE_TEMPORAL_PIC0_BELOW_VALID)                 );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(0, TOPAZHP_CR_REF_PIC0_VALID)                                  );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(0, TOPAZHP_CR_REF_PIC1_VALID)                                  );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(!bIsBPicture, TOPAZHP_CR_REF_PIC1_EQUAL_PIC0)                  );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(bFieldMode ? 1 : 0 , TOPAZHP_CR_FIELD_MODE)                    );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(ui8SwapChromas, TOPAZHP_CR_FRAME_STORE_CHROMA_SWAP)            );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(ui32FrameStoreFormat, TOPAZHP_CR_FRAME_STORE_FORMAT)           );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(TOPAZHP_CR_ENCODER_STANDARD_H264, TOPAZHP_CR_ENCODER_STANDARD) );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s ui32SeqConfig 0x%08x\n", __FUNCTION__, F_ENCODE(uiDeblockIDC == 1 ? 0 : 1, TOPAZHP_CR_DEBLOCK_ENABLE));
}

static IMG_UINT32 ptg__prepare_encode_sliceparams(
    context_ENC_p ctx,
    IMG_UINT32  ui32SliceBufIdx,
    IMG_UINT32  ui32SliceType,
    IMG_UINT16  ui16CurrentRow,
    IMG_UINT16  ui16SliceHeight,
    IMG_UINT8   uiDeblockIDC,
    IMG_BOOL    bFieldMode,
    IMG_INT     iFineYSearchSize,
    IMG_UINT32  ui32StreamIndex
)
{
    IMG_UINT32      ui32FrameStoreFormat;
    IMG_UINT8       ui8SwapChromas;
    IMG_UINT32      ui32MBsPerKick, ui32KicksPerSlice;
    IMG_UINT32      ui32IPEControl;
    IMG_UINT32      ui32Flags = 0;
    IMG_UINT32      ui32SliceConfig = 0;
    IMG_UINT32      ui32SeqConfig = 0;
    IMG_BOOL bIsIntra = IMG_FALSE;
    IMG_BOOL bIsBPicture = IMG_FALSE;
    IMG_BOOL bIsIDR = IMG_FALSE;
    IMG_IPE_MINBLOCKSIZE blkSz;
    IMG_FRAME_TEMPLATE_TYPE eSliceType = (IMG_FRAME_TEMPLATE_TYPE)ui32SliceType;

    if (!ctx) {
        return VA_STATUS_ERROR_INVALID_CONTEXT;
    }

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);
#endif
    /* We want multiple ones of these so we can submit multiple slices without having to wait for the next*/
    ui32IPEControl = ctx->ui32IPEControl;
    bIsIntra = ((eSliceType == IMG_FRAME_IDR) || (eSliceType == IMG_FRAME_INTRA));
    bIsBPicture = (eSliceType == IMG_FRAME_INTER_B);
    bIsIDR = ((eSliceType == IMG_FRAME_IDR) || (eSliceType == IMG_FRAME_INTER_P_IDR));

#ifdef _PDUMP_SLICE_
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s PTG bIsIntra  = %x\n", __FUNCTION__, bIsIntra);
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s PTG bIsBFrame = %x\n", __FUNCTION__, bIsBPicture);
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s PTG bIsIDR    = %x\n", __FUNCTION__, bIsIDR);
#endif
    /* extract block size */
    blkSz = F_EXTRACT(ui32IPEControl, TOPAZHP_CR_IPE_BLOCKSIZE);
    /* mask-out the block size bits from ui32IPEControl */
    ui32IPEControl &= ~(F_MASK(TOPAZHP_CR_IPE_BLOCKSIZE));
    switch (ctx->eStandard) {
    case IMG_STANDARD_NONE:
    case IMG_STANDARD_JPEG:
        break;

    case IMG_STANDARD_H264:
        if (blkSz > 2) blkSz = 2;
        if (bIsBPicture) {
            if (blkSz > 1) blkSz = 1;
        }
#ifdef BRN_30322
        else if (bIsIntra) {
            if (blkSz == 0) blkSz = 1; // Workaround for BRN 30322
        }
#endif

#ifdef BRN_30550
        if (ctx->bCabacEnabled)
            if (blkSz == 0) blkSz = 1;
#endif
        if (ctx->uMBspS >= _1080P_30FPS) {
            ui32IPEControl |= F_ENCODE(iFineYSearchSize, TOPAZHP_CR_IPE_LRITC_BOUNDARY) |
                              F_ENCODE(iFineYSearchSize, TOPAZHP_CR_IPE_Y_FINE_SEARCH);
        } else {
            ui32IPEControl |= F_ENCODE(iFineYSearchSize + 1, TOPAZHP_CR_IPE_LRITC_BOUNDARY) |
                              F_ENCODE(iFineYSearchSize, TOPAZHP_CR_IPE_Y_FINE_SEARCH);

        }
        if (ctx->bLimitNumVectors)
            ui32IPEControl |= F_ENCODE(1, TOPAZHP_CR_IPE_MV_NUMBER_RESTRICTION);
        break;

    case IMG_STANDARD_H263:
        blkSz = 0;
        ui32IPEControl = F_ENCODE(iFineYSearchSize + 1, TOPAZHP_CR_IPE_LRITC_BOUNDARY) |
                         F_ENCODE(iFineYSearchSize, TOPAZHP_CR_IPE_Y_FINE_SEARCH) |
                         F_ENCODE(0, TOPAZHP_CR_IPE_4X4_SEARCH);
        //We only support a maxium vector of 15.5 pixels in H263
        break;

    case IMG_STANDARD_MPEG4:
        if (blkSz > BLK_SZ_8x8) blkSz = BLK_SZ_8x8;
        ui32IPEControl |= F_ENCODE(iFineYSearchSize + 1, TOPAZHP_CR_IPE_LRITC_BOUNDARY) |
                          F_ENCODE(iFineYSearchSize, TOPAZHP_CR_IPE_Y_FINE_SEARCH) |
                          F_ENCODE(0, TOPAZHP_CR_IPE_4X4_SEARCH);
        // FIXME Should be 1, set to zero for hardware testing.
        break;
    case IMG_STANDARD_MPEG2:
        if (blkSz != BLK_SZ_16x16) blkSz = BLK_SZ_16x16;
        ui32IPEControl |= F_ENCODE(iFineYSearchSize + 1, TOPAZHP_CR_IPE_LRITC_BOUNDARY) |
                          F_ENCODE(iFineYSearchSize, TOPAZHP_CR_IPE_Y_FINE_SEARCH) |
                          F_ENCODE(0, TOPAZHP_CR_IPE_4X4_SEARCH);
        // FIXME Should be 1, set to zero for hardware testing.
        break;
    }

    {
        IMG_BOOL bRestrict4x4SearchSize;
        IMG_UINT32 uLritcBoundary;

        if (ctx->uMBspS >= _1080P_30FPS)
            bRestrict4x4SearchSize = 1;
        else
            bRestrict4x4SearchSize = 0;

        ui32IPEControl |= F_ENCODE(blkSz, TOPAZHP_CR_IPE_BLOCKSIZE);
        uLritcBoundary = (blkSz != BLK_SZ_16x16) ? (iFineYSearchSize + (bRestrict4x4SearchSize ? 0 : 1)) : 1;
        if (uLritcBoundary > 3) {
            return VA_STATUS_ERROR_UNKNOWN;
        }

        /* Minium sub block size to calculate motion vectors for. 0=16x16, 1=8x8, 2=4x4 */
        ui32IPEControl = F_INSERT(ui32IPEControl, blkSz, TOPAZHP_CR_IPE_BLOCKSIZE);
        ui32IPEControl = F_INSERT(ui32IPEControl, iFineYSearchSize, TOPAZHP_CR_IPE_Y_FINE_SEARCH);
        ui32IPEControl = F_INSERT(ui32IPEControl, ctx->bLimitNumVectors, TOPAZHP_CR_IPE_MV_NUMBER_RESTRICTION);

        ui32IPEControl = F_INSERT(ui32IPEControl, uLritcBoundary, TOPAZHP_CR_IPE_LRITC_BOUNDARY);  // 8x8 search
        ui32IPEControl = F_INSERT(ui32IPEControl, bRestrict4x4SearchSize ? 0 : 1, TOPAZHP_CR_IPE_4X4_SEARCH);

    }
    ui32IPEControl = F_INSERT(ui32IPEControl, ctx->bHighLatency, TOPAZHP_CR_IPE_HIGH_LATENCY);
//              psSliceParams->ui32IPEControl = ui32IPEControl;

    if (!bIsIntra) {
        if (bIsBPicture)
            ui32Flags |= ISINTERB_FLAGS;
        else
            ui32Flags |= ISINTERP_FLAGS;
    }
    switch (ctx->eStandard)  {
    case IMG_STANDARD_NONE:
        break;
    case IMG_STANDARD_H263:
        ui32Flags |= ISH263_FLAGS;
        break;
    case IMG_STANDARD_MPEG4:
        ui32Flags |= ISMPEG4_FLAGS;
        break;
    case IMG_STANDARD_MPEG2:
        ui32Flags |= ISMPEG2_FLAGS;
        break;
    default:
        break;
    }

    if (ctx->bMultiReferenceP && !(bIsIntra || bIsBPicture))
        ui32Flags |= ISMULTIREF_FLAGS;
    if (ctx->bSpatialDirect && bIsBPicture)
        ui32Flags |= SPATIALDIRECT_FLAGS;

    if (bIsIntra) {
        ui32SliceConfig = F_ENCODE(TOPAZHP_CR_SLICE_TYPE_I_SLICE, TOPAZHP_CR_SLICE_TYPE);
    } else {
        if (bIsBPicture) {
            ui32SliceConfig = F_ENCODE(TOPAZHP_CR_SLICE_TYPE_B_SLICE, TOPAZHP_CR_SLICE_TYPE);
        } else {
            // p frame
            ui32SliceConfig = F_ENCODE(TOPAZHP_CR_SLICE_TYPE_P_SLICE, TOPAZHP_CR_SLICE_TYPE);
        }
    }

    ui32MBsPerKick = ctx->ui32KickSize;
    // we need to figure out the number of kicks and mb's per kick to use.
    // on H.264 we will use a MB's per kick of basic unit
    // on other rc varients we will use mb's per kick of width
    ui32KicksPerSlice = ((ui16SliceHeight / 16) * (ctx->ui16Width / 16)) / ui32MBsPerKick;
    assert((ui32KicksPerSlice * ui32MBsPerKick) == ((ui16SliceHeight / 16)*(ctx->ui16Width / 16)));

    // need some sensible ones don't look to be implemented yet...
    // change per stream

    if ((ctx->eFormat == IMG_CODEC_UY0VY1_8888) || (ctx->eFormat == IMG_CODEC_VY0UY1_8888))
        ui32FrameStoreFormat = 3;
    else if ((ctx->eFormat == IMG_CODEC_Y0UY1V_8888) || (ctx->eFormat == IMG_CODEC_Y0VY1U_8888))
        ui32FrameStoreFormat = 2;
    else if ((ctx->eFormat == IMG_CODEC_PL12) || (ctx->eFormat == IMG_CODEC_422_PL12))
        ui32FrameStoreFormat = 1;
    else
        ui32FrameStoreFormat = 0;

    if ((ctx->eFormat == IMG_CODEC_VY0UY1_8888) || (ctx->eFormat == IMG_CODEC_Y0VY1U_8888))
        ui8SwapChromas = 1;
    else
        ui8SwapChromas = 0;

    switch (ctx->eStandard) {
    case IMG_STANDARD_NONE:
    case IMG_STANDARD_JPEG:
        break;
    case IMG_STANDARD_H264:
        /* H264 */

        ui32SeqConfig = F_ENCODE(0, TOPAZHP_CR_TEMPORAL_PIC0_BELOW_IN_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_TEMPORAL_PIC1_BELOW_IN_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_ABOVE_OUT_OF_SLICE_VALID)
                        | F_ENCODE(1, TOPAZHP_CR_WRITE_TEMPORAL_PIC0_BELOW_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_REF_PIC0_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_REF_PIC1_VALID)
                        | F_ENCODE(!bIsBPicture, TOPAZHP_CR_REF_PIC1_EQUAL_PIC0)
                        | F_ENCODE(bFieldMode ? 1 : 0 , TOPAZHP_CR_FIELD_MODE)
                        | F_ENCODE(ui8SwapChromas, TOPAZHP_CR_FRAME_STORE_CHROMA_SWAP)
                        | F_ENCODE(ui32FrameStoreFormat, TOPAZHP_CR_FRAME_STORE_FORMAT)
                        | F_ENCODE(TOPAZHP_CR_ENCODER_STANDARD_H264, TOPAZHP_CR_ENCODER_STANDARD)
                        | F_ENCODE(uiDeblockIDC == 1 ? 0 : 1, TOPAZHP_CR_DEBLOCK_ENABLE);

        if (ctx->sRCParams.ui16BFrames) {
            ui32SeqConfig |= F_ENCODE(1, TOPAZHP_CR_WRITE_TEMPORAL_COL_VALID);
            if ((ui32Flags & ISINTERB_FLAGS) == ISINTERB_FLAGS)
                ui32SeqConfig |= F_ENCODE(1, TOPAZHP_CR_TEMPORAL_COL_IN_VALID);
        }

        if (!bIsBPicture) {
            ui32SeqConfig |= F_ENCODE(1, TOPAZHP_CR_WRITE_TEMPORAL_COL_VALID);
        }

        break;
    case IMG_STANDARD_MPEG4:
        /* MPEG4 */
        ui32SeqConfig = F_ENCODE(1, TOPAZHP_CR_WRITE_RECON_PIC)
                        | F_ENCODE(0, TOPAZHP_CR_DEBLOCK_ENABLE)
                        | F_ENCODE(0, TOPAZHP_CR_TEMPORAL_PIC0_BELOW_IN_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_TEMPORAL_PIC1_BELOW_IN_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_ABOVE_OUT_OF_SLICE_VALID)
                        | F_ENCODE(((ui32Flags & ISINTERP_FLAGS) == ISINTERP_FLAGS), TOPAZHP_CR_WRITE_TEMPORAL_PIC0_BELOW_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_REF_PIC0_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_REF_PIC1_VALID)
                        | F_ENCODE(1, TOPAZHP_CR_REF_PIC1_EQUAL_PIC0)
                        | F_ENCODE(0, TOPAZHP_CR_FIELD_MODE)
                        | F_ENCODE(ui8SwapChromas, TOPAZHP_CR_FRAME_STORE_CHROMA_SWAP)
                        | F_ENCODE(ui32FrameStoreFormat, TOPAZHP_CR_FRAME_STORE_FORMAT)
                        | F_ENCODE(TOPAZHP_CR_ENCODER_STANDARD_MPEG4, TOPAZHP_CR_ENCODER_STANDARD);
        break;
    case IMG_STANDARD_MPEG2:
        /* MPEG2 */
        ui32SeqConfig = F_ENCODE(1, TOPAZHP_CR_WRITE_RECON_PIC)
                        | F_ENCODE(0, TOPAZHP_CR_DEBLOCK_ENABLE)
                        | F_ENCODE(0, TOPAZHP_CR_TEMPORAL_PIC0_BELOW_IN_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_TEMPORAL_PIC1_BELOW_IN_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_ABOVE_OUT_OF_SLICE_VALID)
                        | F_ENCODE(((ui32Flags & ISINTERP_FLAGS) == ISINTERP_FLAGS), TOPAZHP_CR_WRITE_TEMPORAL_PIC0_BELOW_VALID)
                        | F_ENCODE(1, TOPAZHP_CR_REF_PIC0_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_REF_PIC1_VALID)
                        | F_ENCODE(1, TOPAZHP_CR_REF_PIC1_EQUAL_PIC0)
                        | F_ENCODE(bFieldMode ? 1 : 0 , TOPAZHP_CR_FIELD_MODE)
                        | F_ENCODE(ui8SwapChromas, TOPAZHP_CR_FRAME_STORE_CHROMA_SWAP)
                        | F_ENCODE(ui32FrameStoreFormat, TOPAZHP_CR_FRAME_STORE_FORMAT)
                        | F_ENCODE(TOPAZHP_CR_ENCODER_STANDARD_MPEG2, TOPAZHP_CR_ENCODER_STANDARD);
        break;
    case IMG_STANDARD_H263:
        /* H263 */
        ui32SeqConfig = F_ENCODE(1, TOPAZHP_CR_WRITE_RECON_PIC)
                        | F_ENCODE(0, TOPAZHP_CR_DEBLOCK_ENABLE)
                        | F_ENCODE(0, TOPAZHP_CR_TEMPORAL_PIC0_BELOW_IN_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_TEMPORAL_PIC1_BELOW_IN_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_ABOVE_OUT_OF_SLICE_VALID)
                        | F_ENCODE(((ui32Flags & ISINTERP_FLAGS) == ISINTERP_FLAGS), TOPAZHP_CR_WRITE_TEMPORAL_PIC0_BELOW_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_REF_PIC0_VALID)
                        | F_ENCODE(0, TOPAZHP_CR_REF_PIC1_VALID)
                        | F_ENCODE(1, TOPAZHP_CR_REF_PIC1_EQUAL_PIC0)
                        | F_ENCODE(0, TOPAZHP_CR_FIELD_MODE)
                        | F_ENCODE(ui8SwapChromas, TOPAZHP_CR_FRAME_STORE_CHROMA_SWAP)
                        | F_ENCODE(ui32FrameStoreFormat, TOPAZHP_CR_FRAME_STORE_FORMAT)
                        | F_ENCODE(TOPAZHP_CR_ENCODER_STANDARD_H263, TOPAZHP_CR_ENCODER_STANDARD);
        break;
    }

    if (bIsBPicture)        {
        ui32SeqConfig |= F_ENCODE(0, TOPAZHP_CR_TEMPORAL_PIC1_BELOW_IN_VALID)
                         | F_ENCODE(0, TOPAZHP_CR_WRITE_TEMPORAL_PIC1_BELOW_VALID)
                         | F_ENCODE(1, TOPAZHP_CR_REF_PIC1_VALID)
                         | F_ENCODE(1, TOPAZHP_CR_TEMPORAL_COL_IN_VALID);
    }

    if (ctx->ui8EnableSelStatsFlags & ESF_FIRST_STAGE_STATS)        {
        ui32SeqConfig |= F_ENCODE(1, TOPAZHP_CR_WRITE_MB_FIRST_STAGE_VALID);
    }

    if (ctx->ui8EnableSelStatsFlags & ESF_MP_BEST_MB_DECISION_STATS ||
        ctx->ui8EnableSelStatsFlags & ESF_MP_BEST_MOTION_VECTOR_STATS)  {
        ui32SeqConfig |= F_ENCODE(1, TOPAZHP_CR_BEST_MULTIPASS_OUT_VALID);

        if (!(ctx->ui8EnableSelStatsFlags & ESF_MP_BEST_MOTION_VECTOR_STATS)) {
            ui32SeqConfig |= F_ENCODE(1, TOPAZHP_CR_BEST_MVS_OUT_DISABLE);// 64 Byte Best Multipass Motion Vector output disabled by default
        }
    }

    if (ctx->bEnableInpCtrl) {
        ui32SeqConfig |= F_ENCODE(1, TOPAZHP_CR_MB_CONTROL_IN_VALID);
    }

    if (eSliceType == IMG_FRAME_IDR) {
        ctx->sBiasTables.ui32SeqConfigInit = ui32SeqConfig;
    }

    ptg__save_slice_params_template(ctx, ui32SliceBufIdx, eSliceType,
        ui32IPEControl, ui32Flags, ui32SliceConfig, ui32SeqConfig, ui32StreamIndex);
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s end\n", __FUNCTION__);
#endif
    return 0;
}

void ptg__mpeg4_generate_pic_hdr_template(
    context_ENC_p ctx,
    IMG_FRAME_TEMPLATE_TYPE ui8SliceType,
    IMG_UINT8 ui8Search_range)
{
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ctx->ui32StreamID]);
    MTX_HEADER_PARAMS * pPicHeaderMem;
    VOP_CODING_TYPE eVop_Coding_Type;
    IMG_BOOL8 b8IsVopCoded;
    IMG_UINT8 ui8OriginalSliceType = ui8SliceType;

    /* MPEG4: We do not support B-frames at the moment, so we use a spare slot, to store a template for the skipped frame */
    if (ui8SliceType == IMG_FRAME_INTER_B)
    {
	ui8SliceType = IMG_FRAME_INTER_P;
	b8IsVopCoded = IMG_FALSE;
    } else {
	b8IsVopCoded = IMG_TRUE;
    }

    eVop_Coding_Type = (ui8SliceType == IMG_FRAME_INTER_P) ? P_FRAME : I_FRAME;
#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_map(&(ps_mem->bufs_pic_template), &(ps_mem->bufs_pic_template.virtual_addr));
#endif
    pPicHeaderMem = (MTX_HEADER_PARAMS *)((IMG_UINT8*)(ps_mem->bufs_pic_template.virtual_addr + (ctx->ctx_mem_size.pic_template * ui8OriginalSliceType)));
    //todo fix time resolution
    ptg__MPEG4_notforsims_prepare_vop_header(pPicHeaderMem, b8IsVopCoded, ui8Search_range, eVop_Coding_Type);
#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_unmap(&(ps_mem->bufs_pic_template));
#endif

}

void ptg__h263_generate_pic_hdr_template(context_ENC_p ctx,
       IMG_FRAME_TEMPLATE_TYPE eFrameType)
{
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ctx->ui32StreamID]);
    MTX_HEADER_PARAMS * pPicHeaderMem = NULL;
    H263_PICTURE_CODING_TYPE ePictureCodingType = ((eFrameType == IMG_FRAME_INTRA)|| (eFrameType == IMG_FRAME_IDR)) ? I_FRAME : P_FRAME;
       
#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_map(&(ps_mem->bufs_pic_template), &(ps_mem->bufs_pic_template.virtual_addr));
#endif
    pPicHeaderMem = (MTX_HEADER_PARAMS *)((IMG_UINT8*)(ps_mem->bufs_pic_template.virtual_addr + (ctx->ctx_mem_size.pic_template * eFrameType)));

    IMG_UINT8 ui8FrameRate = (IMG_UINT8)ctx->sRCParams.ui32FrameRate;
    IMG_UINT32 ui32PictureWidth = ctx->ui16Width;
    IMG_UINT32 ui32PictureHeigth  = ctx->ui16FrameHeight;

    // Get a pointer to the memory the header will be written to
    ptg__H263_notforsims_prepare_video_pictureheader(
        pPicHeaderMem,
        ePictureCodingType,
        ctx->ui8H263SourceFormat,
        ui8FrameRate,
        ui32PictureWidth,
        ui32PictureHeigth );
#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_unmap(&(ps_mem->bufs_pic_template));
#endif

}

void ptg__generate_slice_params_template(
    context_ENC_p ctx,
    IMG_UINT32 slice_buf_idx,
    IMG_UINT32 slice_type,
    IMG_UINT32 ui32StreamIndex
)
{
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
    IMG_UINT8  *slice_mem_temp_p = NULL;
    IMG_UINT32 ui32SliceHeight = 0;
    IMG_FRAME_TEMPLATE_TYPE slice_temp_type = (IMG_FRAME_TEMPLATE_TYPE)slice_type;
       
    IMG_FRAME_TEMPLATE_TYPE buf_idx = (IMG_FRAME_TEMPLATE_TYPE)slice_buf_idx;

#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_map(&(ps_mem->bufs_slice_template), &(ps_mem->bufs_slice_template.virtual_addr));
#endif
    slice_mem_temp_p = (IMG_UINT8*)(ps_mem->bufs_slice_template.virtual_addr + (ctx->ctx_mem_size.slice_template * buf_idx));
#ifdef _PDUMP_FUNC_
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: addr 0x%08x, virtual 0x%08x, size = 0x%08x, buf_idx = %x\n", __FUNCTION__, slice_mem_temp_p, ps_mem->bufs_slice_template.virtual_addr, ctx->ctx_mem_size.slice_template, buf_idx);
#endif

    if (ctx->ui8SlicesPerPicture != 0)
        ui32SliceHeight = ctx->ui16PictureHeight / ctx->ui8SlicesPerPicture;
    else
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s slice height\n", __FUNCTION__);
    
    ui32SliceHeight &= ~15;
#ifdef _PDUMP_SLICE_
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s PTG ui8DeblockIDC    = %x\n", __FUNCTION__, ctx->ui8DeblockIDC   );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s PTG ui32SliceHeight  = %x\n", __FUNCTION__, ui32SliceHeight );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s PTG bIsInterlaced    = %x\n", __FUNCTION__, ctx->bIsInterlaced   );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s PTG iFineYSearchSize = %x\n", __FUNCTION__, ctx->iFineYSearchSize);
#endif

    ptg__prepare_encode_sliceparams(
        ctx,
	slice_buf_idx,
        slice_temp_type,
        0,                        // ui16CurrentRow,
        ui32SliceHeight,
        ctx->ui8DeblockIDC,       // uiDeblockIDC
        ctx->bIsInterlaced,       // bFieldMode
        ctx->iFineYSearchSize,
        ui32StreamIndex
    );

#ifdef _PDUMP_SLICE_
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s PTG bCabacEnabled  = %x\n", __FUNCTION__, ctx->bCabacEnabled );
  drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s PTG ui16MVCViewIdx = %x\n", __FUNCTION__, ctx->ui16MVCViewIdx);
#endif

    if(ctx->bEnableMVC)
        ctx->ui16MVCViewIdx = (IMG_UINT16)ui32StreamIndex;

    /* Prepare Slice Header Template */
    switch (ctx->eStandard) {
    case IMG_STANDARD_NONE:
    case IMG_STANDARD_JPEG:
    case IMG_STANDARD_MPEG4:
        break;
    case IMG_STANDARD_H264:
        //H264_NOTFORSIMS_PrepareSliceHeader
        ptg__H264ES_notforsims_prepare_sliceheader(
            slice_mem_temp_p,
            slice_temp_type,
            ctx->ui8DeblockIDC,
            0,                   // ui32FirstMBAddress
            0,                   // uiMBSkipRun
            ctx->bCabacEnabled,
            ctx->bIsInterlaced,
            ctx->ui16MVCViewIdx, //(IMG_UINT16)(NON_MVC_VIEW);
            IMG_FALSE            // bIsLongTermRef
        );
        break;

    case IMG_STANDARD_H263:
        ptg__H263ES_notforsims_prepare_gobsliceheader(slice_mem_temp_p);
        break;

    case IMG_STANDARD_MPEG2:
        ptg__MPEG2_prepare_sliceheader(slice_mem_temp_p);
        break;
    }

#ifdef _TOPAZHP_VIR_ADDR_
        psb_buffer_unmap(&(ps_mem->bufs_slice_template));
#endif

#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: end \n", __FUNCTION__);
#endif
    return ;
}

//H264_PrepareTemplates
static VAStatus ptg__prepare_templates(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    IMG_RC_PARAMS *psRCParams = &(ctx->sRCParams);
    PIC_PARAMS *psPicParams = &(ctx->sPicParams);
    IN_RC_PARAMS* psInParams = &(psPicParams->sInParams);
//    IMG_UINT8  *slice_mem_temp_p = NULL;

    psPicParams->ui32Flags = 0;
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);
#endif

    ptg__prepare_mv_estimates(ctx, ui32StreamIndex);

    switch (ctx->eStandard) {
        case IMG_STANDARD_H263:
            psPicParams->ui32Flags |= ISH263_FLAGS;
            break;
        case IMG_STANDARD_MPEG4:
            psPicParams->ui32Flags |= ISMPEG4_FLAGS;
            break;
        case IMG_STANDARD_MPEG2:
            psPicParams->ui32Flags |= ISMPEG2_FLAGS;
            break;
        default:
            break;
    }

    if (psRCParams->bRCEnable) {
        psPicParams->ui32Flags |= ISRC_FLAGS;
        ptg__setup_rcdata(ctx);
    } else {
        psInParams->ui8SeInitQP  = psRCParams->ui32InitialQp;
        psInParams->ui8MBPerRow  = (ctx->ui16Width >> 4);
        psInParams->ui16MBPerFrm = (ctx->ui16Width >> 4) * (ctx->ui16PictureHeight >> 4);
        psInParams->ui16MBPerBU  = psRCParams->ui32BUSize;
        psInParams->ui16BUPerFrm = (psInParams->ui16MBPerFrm) / psRCParams->ui32BUSize;
        ctx->ui32KickSize = psInParams->ui16MBPerBU;
    }

    // Prepare Slice header templates
    ptg__generate_slice_params_template(ctx, (IMG_UINT32)IMG_FRAME_IDR, (IMG_UINT32)IMG_FRAME_IDR, ui32StreamIndex);
    ptg__generate_slice_params_template(ctx, (IMG_UINT32)IMG_FRAME_INTRA, (IMG_UINT32)IMG_FRAME_INTRA, ui32StreamIndex);
    ptg__generate_slice_params_template(ctx, (IMG_UINT32)IMG_FRAME_INTER_P, (IMG_UINT32)IMG_FRAME_INTER_P, ui32StreamIndex);
    switch(ctx->eStandard) {
	case IMG_STANDARD_H264:
	    ptg__generate_slice_params_template(ctx, (IMG_UINT32)IMG_FRAME_INTER_B, (IMG_UINT32)IMG_FRAME_INTER_B, ui32StreamIndex);
	    if (ctx->bEnableMVC)
		ptg__generate_slice_params_template(ctx, (IMG_UINT32)IMG_FRAME_INTER_P_IDR, (IMG_UINT32)IMG_FRAME_INTER_P_IDR, ui32StreamIndex);
	    break;
	case IMG_STANDARD_H263:
	    ptg__generate_slice_params_template(ctx, (IMG_UINT32)IMG_FRAME_INTER_B, (IMG_UINT32)IMG_FRAME_INTER_B, ui32StreamIndex);
	    ptg__h263_generate_pic_hdr_template(ctx, IMG_FRAME_IDR);
            ptg__h263_generate_pic_hdr_template(ctx, IMG_FRAME_INTRA);
            ptg__h263_generate_pic_hdr_template(ctx, IMG_FRAME_INTER_P);
	    ptg__h263_generate_pic_hdr_template(ctx, IMG_FRAME_INTER_B);
	    break;
	case IMG_STANDARD_MPEG4:
	    ptg__generate_slice_params_template(ctx, (IMG_UINT32)IMG_FRAME_INTER_B, (IMG_UINT32)IMG_FRAME_INTER_P, ui32StreamIndex);
	    ptg__mpeg4_generate_pic_hdr_template(ctx, IMG_FRAME_IDR, 4);
            ptg__mpeg4_generate_pic_hdr_template(ctx, IMG_FRAME_INTRA, 4);
            ptg__mpeg4_generate_pic_hdr_template(ctx, IMG_FRAME_INTER_P, 4);
	    ptg__mpeg4_generate_pic_hdr_template(ctx, IMG_FRAME_INTER_B, 4);
	    break;
	default:
	    break;
    }

    //FIXME: Zhaohan ptg__mpeg2/mpeg4_generate_pic_hdr_template(IMG_FRAME_IDR\IMG_FRAME_INTRA\IMG_FRAME_INTER_P\IMG_FRAME_INTER_B);
 
/*
    else {
        slice_mem_temp_p = (IMG_UINT8*)cmdbuf->slice_mem_p + (((IMG_UINT32)IMG_FRAME_INTER_P_IDR) * cmdbuf->mem_size);
        memset(slice_mem_temp_p, 0, 128);
    }
*/
    // Prepare Pic Params Templates
    ptg__adjust_picflags(ctx, psRCParams, IMG_TRUE, &(ctx->ui32FirstPicFlags));
    ptg__adjust_picflags(ctx, psRCParams, IMG_FALSE, &(ctx->ui32NonFirstPicFlags));

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s end\n", __FUNCTION__);

    return VA_STATUS_SUCCESS;
}


static void ptg__setvideo_params(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
    context_ENC_frame_buf *ps_buf = &(ctx->ctx_frame_buf);
    IMG_MTX_VIDEO_CONTEXT* psMtxEncContext = NULL;

#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_map(&(ps_mem->bufs_mtx_context), &(ps_mem->bufs_mtx_context.virtual_addr));
#endif
    psMtxEncContext = (IMG_MTX_VIDEO_CONTEXT*)(ps_mem->bufs_mtx_context.virtual_addr);
    IMG_RC_PARAMS * psRCParams = &(ctx->sRCParams);
    IMG_INT nIndex;
    IMG_UINT8 ui8Flag;

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);

#ifndef EXCLUDE_ADAPTIVE_ROUNDING
    IMG_INT32 i, j;
#endif

    ctx->i32PicNodes = (psRCParams->b16Hierarchical ? MAX_REF_B_LEVELS : 0) + ctx->ui8RefSpacing + 4;
    ctx->i32MVStores = (ctx->i32PicNodes * 2);
    ctx->ui8SlotsRequired = ctx->ui8SlotsInUse = psRCParams->ui16BFrames + 2;

    psMtxEncContext->ui32InitialQp = ctx->sRCParams.ui32InitialQp;
    psMtxEncContext->ui32BUSize = ctx->sRCParams.ui32BUSize;
    psMtxEncContext->ui16CQPOffset = (ctx->sRCParams.i8QCPOffset & 0x1f) | ((ctx->sRCParams.i8QCPOffset & 0x1f) << 8);
    psMtxEncContext->eStandard = ctx->eStandard;
    psMtxEncContext->ui32WidthInMbs = (ctx->ui16Width + 15) >> 4;
    psMtxEncContext->ui32PictureHeightInMbs = (ctx->ui16PictureHeight + 15) >> 4;
    psMtxEncContext->bOutputReconstructed = ctx->bOutputReconstructed;
    psMtxEncContext->ui32VopTimeResolution = ctx->ui32VopTimeResolution;
    psMtxEncContext->ui8MaxSlicesPerPicture = ctx->ui8SlicesPerPicture;
    psMtxEncContext->ui8NumPipes = (IMG_UINT8)(ctx->i32NumPipes);
    psMtxEncContext->eFormat = ctx->eFormat;

    psMtxEncContext->b8IsInterlaced = ctx->bIsInterlaced;
    psMtxEncContext->b8TopFieldFirst = ctx->bTopFieldFirst;
    psMtxEncContext->b8ArbitrarySO = ctx->bArbitrarySO;

    psMtxEncContext->ui32IdrPeriod = ctx->ui32IdrPeriod * ctx->ui32IntraCnt;
    psMtxEncContext->ui32BFrameCount = ctx->sRCParams.ui16BFrames;
    psMtxEncContext->b8Hierarchical = (IMG_BOOL8) ctx->sRCParams.b16Hierarchical;
    psMtxEncContext->ui32IntraLoopCnt = ctx->ui32IntraCnt;
    psMtxEncContext->ui8RefSpacing = ctx->ui8RefSpacing;
    psMtxEncContext->ui32DebugCRCs = ctx->ui32DebugCRCs;

    psMtxEncContext->ui8FirstPipe = ctx->ui8BasePipe;
    psMtxEncContext->ui8LastPipe = ctx->ui8BasePipe + psMtxEncContext->ui8NumPipes - 1;
    psMtxEncContext->ui8PipesToUseFlags = 0;
    ui8Flag = 0x1 << psMtxEncContext->ui8FirstPipe;
    for (nIndex = 0; nIndex < psMtxEncContext->ui8NumPipes; nIndex++, ui8Flag<<=1)
        psMtxEncContext->ui8PipesToUseFlags |= ui8Flag; //Pipes used MUST be contiguous from the BasePipe offset

    // Only used in MPEG2, 2 bit field (0 = 8 bit, 1 = 9 bit, 2 = 10 bit and 3=11 bit precision)
    if (ctx->eStandard == IMG_STANDARD_MPEG2)
        psMtxEncContext->ui8MPEG2IntraDCPrecision = ctx->ui8MPEG2IntraDCPrecision;

    psMtxEncContext->b16EnableMvc = ctx->bEnableMVC;
    psMtxEncContext->ui16MvcViewIdx = ui32StreamIndex;
    if (ctx->eStandard == IMG_STANDARD_H264)
        psMtxEncContext->bNoSequenceHeaders = ctx->bNoSequenceHeaders;

    {
        IMG_UINT16 ui16SrcYStride = 0, ui16SrcUVStride = 0;
        // 3 Components: Y, U, V
        ctx->ui16BufferStride = ps_buf->src_surface->psb_surface->stride;
        ui16SrcUVStride = ui16SrcYStride = ctx->ui16BufferStride;
        ctx->ui16BufferHeight = ctx->ui16FrameHeight;
        switch (ctx->eFormat) {
        case IMG_CODEC_YUV:
        case IMG_CODEC_PL8:
        case IMG_CODEC_YV12:
            ui16SrcUVStride = ui16SrcYStride / 2;
            break;

        case IMG_CODEC_PL12:            // Interleaved chroma pixels
        case IMG_CODEC_IMC2:            // Interleaved chroma rows
        case IMG_CODEC_422_YUV:         // Odd-numbered chroma rows unused
        case IMG_CODEC_422_YV12:        // Odd-numbered chroma rows unused
        case IMG_CODEC_422_PL8:         // Odd-numbered chroma rows unused
        case IMG_CODEC_Y0UY1V_8888: // Interleaved luma and chroma pixels
        case IMG_CODEC_Y0VY1U_8888: // Interleaved luma and chroma pixels
        case IMG_CODEC_UY0VY1_8888: // Interleaved luma and chroma pixels
        case IMG_CODEC_VY0UY1_8888: // Interleaved luma and chroma pixels
            ui16SrcUVStride = ui16SrcYStride;
            break;

        case IMG_CODEC_422_IMC2:        // Interleaved chroma pixels and unused odd-numbered chroma rows
        case IMG_CODEC_422_PL12:        // Interleaved chroma rows and unused odd-numbered chroma rows
            ui16SrcUVStride = ui16SrcYStride * 2;
            break;

        default:
            break;
        }

        if ((ctx->bIsInterlaced) && (ctx->bIsInterleaved)) {
            ui16SrcYStride *= 2;                    // ui16SrcYStride
            ui16SrcUVStride *= 2;           // ui16SrcUStride
        }

        psMtxEncContext->ui32PicRowStride = F_ENCODE(ui16SrcYStride >> 6, TOPAZHP_CR_CUR_PIC_LUMA_STRIDE) |
                                            F_ENCODE(ui16SrcUVStride >> 6, TOPAZHP_CR_CUR_PIC_CHROMA_STRIDE);
    }

    psMtxEncContext->eRCMode = ctx->sRCParams.eRCMode;
    psMtxEncContext->b8DisableBitStuffing = ctx->sRCParams.bDisableBitStuffing;
    psMtxEncContext->b8FirstPic = IMG_TRUE;

    /*Contents Adaptive Rate Control Parameters*/
    psMtxEncContext->bCARC          =  ctx->sCARCParams.bCARC;
    psMtxEncContext->iCARCBaseline  =  ctx->sCARCParams.i32CARCBaseline;
    psMtxEncContext->uCARCThreshold =  ctx->sCARCParams.ui32CARCThreshold;
    psMtxEncContext->uCARCCutoff    =  ctx->sCARCParams.ui32CARCCutoff;
    psMtxEncContext->uCARCNegRange  =  ctx->sCARCParams.ui32CARCNegRange;
    psMtxEncContext->uCARCNegScale  =  ctx->sCARCParams.ui32CARCNegScale;
    psMtxEncContext->uCARCPosRange  =  ctx->sCARCParams.ui32CARCPosRange;
    psMtxEncContext->uCARCPosScale  =  ctx->sCARCParams.ui32CARCPosScale;
    psMtxEncContext->uCARCShift     =  ctx->sCARCParams.ui32CARCShift;
    psMtxEncContext->ui32MVClip_Config =  F_ENCODE(ctx->bNoOffscreenMv, TOPAZHP_CR_MVCALC_RESTRICT_PICTURE);
    psMtxEncContext->ui32LRITC_Tile_Use_Config = F_ENCODE(-1, TOPAZHP_CR_MAX_PIC0_LUMA_TILES)
        | F_ENCODE(-1, TOPAZHP_CR_MAX_PIC1_LUMA_TILES)
        | F_ENCODE(-1, TOPAZHP_CR_MAX_PIC0_CHROMA_TILES)
        | F_ENCODE(-1, TOPAZHP_CR_MAX_PIC1_CHROMA_TILES);
    psMtxEncContext->ui32LRITC_Cache_Chunk_Config = 0;

    psMtxEncContext->ui32IPCM_0_Config = F_ENCODE(ctx->ui32CabacBinFlex, TOPAZ_VLC_CR_CABAC_BIN_FLEX) |
        F_ENCODE(DEFAULT_CABAC_DB_MARGIN, TOPAZ_VLC_CR_CABAC_DB_MARGIN);

    psMtxEncContext->ui32IPCM_1_Config = F_ENCODE(3200, TOPAZ_VLC_CR_IPCM_THRESHOLD) |
        F_ENCODE(ctx->ui32CabacBinLimit, TOPAZ_VLC_CR_CABAC_BIN_LIMIT);

    // leave alone until high profile and constrained modes are defined.
    psMtxEncContext->ui32H264CompControl  = F_ENCODE((ctx->bCabacEnabled ? 0 : 1), TOPAZHP_CR_H264COMP_8X8_CAVLC);  

    psMtxEncContext->ui32H264CompControl |= F_ENCODE(ctx->bUseDefaultScalingList ? 1 : 0, TOPAZHP_CR_H264COMP_DEFAULT_SCALING_LIST);

    psMtxEncContext->ui32H264CompControl |= F_ENCODE(ctx->bH2648x8Transform ? 1 : 0, TOPAZHP_CR_H264COMP_8X8_TRANSFORM);

    psMtxEncContext->ui32H264CompControl |= F_ENCODE(ctx->bH264IntraConstrained ? 1 : 0, TOPAZHP_CR_H264COMP_CONSTRAINED_INTRA);


#ifndef EXCLUDE_ADAPTIVE_ROUNDING
    psMtxEncContext->bMCAdaptiveRoundingDisable = ctx->bVPAdaptiveRoundingDisable;
    psMtxEncContext->ui32H264CompControl |= F_ENCODE(psMtxEncContext->bMCAdaptiveRoundingDisable ? 0 : 1, TOPAZHP_CR_H264COMP_ADAPT_ROUND_ENABLE);

    if (!psMtxEncContext->bMCAdaptiveRoundingDisable)
        for (i = 0; i < 4; i++)
            for (j = 0; j < 18; j++)
                psMtxEncContext->ui16MCAdaptiveRoundingOffsets[j][i] = H264_ROUNDING_OFFSETS[j][i];
#endif

    if ((ctx->eStandard == IMG_STANDARD_H264) && (ctx->ui32CoreRev >= MIN_34_REV)) {
        psMtxEncContext->ui32H264CompControl |= F_ENCODE(USE_VCM_HW_SUPPORT, TOPAZHP_CR_H264COMP_VIDEO_CONF_ENABLE);
    }

    psMtxEncContext->ui32H264CompControl |=
           F_ENCODE(ctx->ui16UseCustomScalingLists & 0x01 ? 1 : 0, TOPAZHP_CR_H264COMP_CUSTOM_QUANT_4X4_INTRA_LUMA_ENABLE)
        | F_ENCODE(ctx->ui16UseCustomScalingLists & 0x02 ? 1 : 0, TOPAZHP_CR_H264COMP_CUSTOM_QUANT_4X4_INTRA_CB_ENABLE)
        | F_ENCODE(ctx->ui16UseCustomScalingLists & 0x04 ? 1 : 0, TOPAZHP_CR_H264COMP_CUSTOM_QUANT_4X4_INTRA_CR_ENABLE)
        | F_ENCODE(ctx->ui16UseCustomScalingLists & 0x08 ? 1 : 0, TOPAZHP_CR_H264COMP_CUSTOM_QUANT_4X4_INTER_LUMA_ENABLE)
        | F_ENCODE(ctx->ui16UseCustomScalingLists & 0x10 ? 1 : 0, TOPAZHP_CR_H264COMP_CUSTOM_QUANT_4X4_INTER_CB_ENABLE)
        | F_ENCODE(ctx->ui16UseCustomScalingLists & 0x20 ? 1 : 0, TOPAZHP_CR_H264COMP_CUSTOM_QUANT_4X4_INTER_CR_ENABLE)
        | F_ENCODE(ctx->ui16UseCustomScalingLists & 0x40 ? 1 : 0, TOPAZHP_CR_H264COMP_CUSTOM_QUANT_8X8_INTRA_LUMA_ENABLE)
        | F_ENCODE(ctx->ui16UseCustomScalingLists & 0x80 ? 1 : 0, TOPAZHP_CR_H264COMP_CUSTOM_QUANT_8X8_INTER_LUMA_ENABLE);

    psMtxEncContext->ui32H264CompControl |=
           F_ENCODE(ctx->bEnableLossless ? 1 : 0, TOPAZHP_CR_H264COMP_LOSSLESS)
        | F_ENCODE(ctx->bLossless8x8Prefilter ? 1 : 0, TOPAZHP_CR_H264COMP_LOSSLESS_8X8_PREFILTER);

    psMtxEncContext->ui32H264CompIntraPredModes = 0x3ffff;// leave at default for now.
    psMtxEncContext->ui32PredCombControl = ctx->ui32PredCombControl;
    psMtxEncContext->ui32SkipCodedInterIntra = F_ENCODE(ctx->ui8InterIntraIndex, TOPAZHP_CR_INTER_INTRA_SCALE_IDX)
        |F_ENCODE(ctx->ui8CodedSkippedIndex, TOPAZHP_CR_SKIPPED_CODED_SCALE_IDX);

    if (ctx->bEnableInpCtrl) {
        psMtxEncContext->ui32MBHostCtrl = F_ENCODE(ctx->bEnableHostQP, TOPAZHP_CR_MB_HOST_QP)
            |F_ENCODE(ctx->bEnableHostBias, TOPAZHP_CR_MB_HOST_SKIPPED_CODED_SCALE)
            |F_ENCODE(ctx->bEnableHostBias, TOPAZHP_CR_MB_HOST_INTER_INTRA_SCALE);
        psMtxEncContext->ui32PredCombControl |= F_ENCODE(1, TOPAZHP_CR_INTER_INTRA_SCALE_ENABLE)
            |F_ENCODE(1, TOPAZHP_CR_SKIPPED_CODED_SCALE_ENABLE);
    }

    if (ctx->bEnableCumulativeBiases)
        psMtxEncContext->ui32PredCombControl |= F_ENCODE(1, TOPAZHP_CR_CUMULATIVE_BIASES_ENABLE);

    psMtxEncContext->ui32PredCombControl |=
        F_ENCODE((((ctx->ui8InterIntraIndex == 3) && (ctx->ui8CodedSkippedIndex == 3)) ? 0 : 1), TOPAZHP_CR_INTER_INTRA_SCALE_ENABLE) |
        F_ENCODE((ctx->ui8CodedSkippedIndex == 3 ? 0 : 1), TOPAZHP_CR_SKIPPED_CODED_SCALE_ENABLE);
    // workaround for BRN 33252, if the Skip coded scale is set then we also have to set the inter/inter enable. We set it enabled and rely on the default value of 3 (unbiased) to keep things behaving.
    //      psMtxEncContext->ui32PredCombControl |= F_ENCODE((ctx->ui8InterIntraIndex==3?0:1), TOPAZHP_CR_INTER_INTRA_SCALE_ENABLE) | F_ENCODE((ctx->ui8CodedSkippedIndex==3?0:1), TOPAZHP_CR_SKIPPED_CODED_SCALE_ENABLE);
    //psMtxEncContext->ui32PredCombControl |= F_ENCODE(1, TOPAZHP_CR_INTER_INTRA_SCALE_ENABLE) | F_ENCODE(1, TOPAZHP_CR_SKIPPED_CODED_SCALE_ENABLE);
    psMtxEncContext->ui32DeblockCtrl = F_ENCODE(ctx->ui8DeblockIDC, TOPAZ_DB_CR_DISABLE_DEBLOCK_IDC);
    psMtxEncContext->ui32VLCControl = 0;

    switch (ctx->eStandard) {
    case IMG_STANDARD_H264:
        psMtxEncContext->ui32VLCControl |= F_ENCODE(1, TOPAZ_VLC_CR_CODEC); // 1 for H.264 note this is inconsistant with the sequencer value
        psMtxEncContext->ui32VLCControl |= F_ENCODE(0, TOPAZ_VLC_CR_CODEC_EXTEND);

        break;
    case IMG_STANDARD_H263:
        psMtxEncContext->ui32VLCControl |= F_ENCODE(3, TOPAZ_VLC_CR_CODEC); // 3 for H.263 note this is inconsistant with the sequencer value
        psMtxEncContext->ui32VLCControl |= F_ENCODE(0, TOPAZ_VLC_CR_CODEC_EXTEND);

        break;
    case IMG_STANDARD_MPEG4:
        psMtxEncContext->ui32VLCControl |= F_ENCODE(2, TOPAZ_VLC_CR_CODEC); // 2 for Mpeg4 note this is inconsistant with the sequencer value
        psMtxEncContext->ui32VLCControl |= F_ENCODE(0, TOPAZ_VLC_CR_CODEC_EXTEND);
        break;
    case IMG_STANDARD_MPEG2:
        psMtxEncContext->ui32VLCControl |= F_ENCODE(0, TOPAZ_VLC_CR_CODEC);
        psMtxEncContext->ui32VLCControl |= F_ENCODE(1, TOPAZ_VLC_CR_CODEC_EXTEND);
        break;
    default:
        break;
    }

    if (ctx->bCabacEnabled) {
        psMtxEncContext->ui32VLCControl |= F_ENCODE(1, TOPAZ_VLC_CR_CABAC_ENABLE); // 2 for Mpeg4 note this is inconsistant with the sequencer value
    }

    psMtxEncContext->ui32VLCControl |= F_ENCODE(ctx->bIsInterlaced ? 1 : 0, TOPAZ_VLC_CR_VLC_FIELD_CODED);
    psMtxEncContext->ui32VLCControl |= F_ENCODE(ctx->bH2648x8Transform ? 1 : 0, TOPAZ_VLC_CR_VLC_8X8_TRANSFORM);
    psMtxEncContext->ui32VLCControl |= F_ENCODE(ctx->bH264IntraConstrained ? 1 : 0, TOPAZ_VLC_CR_VLC_CONSTRAINED_INTRA);

    psMtxEncContext->ui32VLCSliceControl = F_ENCODE(ctx->sRCParams.ui32SliceByteLimit, TOPAZ_VLC_CR_SLICE_SIZE_LIMIT);
    psMtxEncContext->ui32VLCSliceMBControl = F_ENCODE(ctx->sRCParams.ui32SliceMBLimit, TOPAZ_VLC_CR_SLICE_MBS_LIMIT);

        switch (ctx->eStandard) {
            case IMG_STANDARD_H264: {
                IMG_UINT32 ui32VertMVLimit = 255; // default to no clipping
                if (ctx->ui32VertMVLimit)
                    ui32VertMVLimit = ctx->ui32VertMVLimit;
                // as topaz can only cope with at most 255 (in the register field)
                ui32VertMVLimit = MIN(255,ui32VertMVLimit);
                // workaround for BRN 29973 and 30032
                {
#if defined(BRN_29973) || defined(BRN_30032)
                    if (ctx->ui32CoreRev <= 0x30006) {
                        if ((ctx->ui16Width / 16) & 1) // BRN 30032, if odd number of macroblocks we need to limit vector to +-112
                            psMtxEncContext->ui32IPEVectorClipping = F_ENCODE(1, TOPAZHP_CR_IPE_VECTOR_CLIPPING_ENABLED)
                                |F_ENCODE(112, TOPAZHP_CR_IPE_VECTOR_CLIPPING_X)
                                |F_ENCODE(ui32VertMVLimit, TOPAZHP_CR_IPE_VECTOR_CLIPPING_Y);
                        else // for 29973 we need to limit vector to +-120
                            if (ctx->ui16Width >= 1920)
                                psMtxEncContext->ui32IPEVectorClipping = F_ENCODE(1, TOPAZHP_CR_IPE_VECTOR_CLIPPING_ENABLED)
                                |F_ENCODE(120, TOPAZHP_CR_IPE_VECTOR_CLIPPING_X)
                                |F_ENCODE(ui32VertMVLimit, TOPAZHP_CR_IPE_VECTOR_CLIPPING_Y);
                            else
                                psMtxEncContext->ui32IPEVectorClipping = F_ENCODE(1, TOPAZHP_CR_IPE_VECTOR_CLIPPING_ENABLED)
                                |F_ENCODE(255, TOPAZHP_CR_IPE_VECTOR_CLIPPING_X)
                                |F_ENCODE(ui32VertMVLimit, TOPAZHP_CR_IPE_VECTOR_CLIPPING_Y);
                    } else
#endif
                        {
                            psMtxEncContext->ui32IPEVectorClipping =
                            F_ENCODE(1, TOPAZHP_CR_IPE_VECTOR_CLIPPING_ENABLED) |
                            F_ENCODE(255, TOPAZHP_CR_IPE_VECTOR_CLIPPING_X) |
                            F_ENCODE(ui32VertMVLimit, TOPAZHP_CR_IPE_VECTOR_CLIPPING_Y);
                        }
                    }
        
                    psMtxEncContext->ui32SPEMvdClipRange = F_ENCODE(0, TOPAZHP_CR_SPE_MVD_CLIP_ENABLE);
                    psMtxEncContext->ui32JMCompControl = F_ENCODE(1, TOPAZHP_CR_JMCOMP_AC_ENABLE);
            }
            break;
            case IMG_STANDARD_H263:
                {
                    psMtxEncContext->ui32IPEVectorClipping = F_ENCODE(1, TOPAZHP_CR_IPE_VECTOR_CLIPPING_ENABLED)
                        | F_ENCODE(16 - 1, TOPAZHP_CR_IPE_VECTOR_CLIPPING_X)
                        | F_ENCODE(16 - 1, TOPAZHP_CR_IPE_VECTOR_CLIPPING_Y);
            
                    psMtxEncContext->ui32SPEMvdClipRange = F_ENCODE(1, TOPAZHP_CR_SPE_MVD_CLIP_ENABLE)
                        | F_ENCODE( 62, TOPAZHP_CR_SPE_MVD_POS_CLIP)
                        | F_ENCODE(-64, TOPAZHP_CR_SPE_MVD_NEG_CLIP);
                    psMtxEncContext->ui32JMCompControl = F_ENCODE(0, TOPAZHP_CR_JMCOMP_AC_ENABLE);
                }
                break;
            case IMG_STANDARD_MPEG4:
            case IMG_STANDARD_MPEG2:
                {
                    IMG_UINT8 uX, uY, uResidualSize;
                    //The maximum MPEG4 and MPEG2 motion vector differential in 1/2 pixels is
                    //calculated as 1 << (fcode - 1) * 32 or *16 in MPEG2
            
                    uResidualSize=(ctx->eStandard == IMG_STANDARD_MPEG4 ? 32 : 16);
            
                    uX = MIN(128 - 1, (((1<<(ctx->sBiasTables.ui32FCode - 1)) * uResidualSize)/4) - 1);
                    uY = MIN(104 - 1, (((1<<(ctx->sBiasTables.ui32FCode - 1)) * uResidualSize)/4) - 1);
            
                    //Write to register
                    psMtxEncContext->ui32IPEVectorClipping = F_ENCODE(1, TOPAZHP_CR_IPE_VECTOR_CLIPPING_ENABLED)
                        | F_ENCODE(uX, TOPAZHP_CR_IPE_VECTOR_CLIPPING_X)
                        | F_ENCODE(uY, TOPAZHP_CR_IPE_VECTOR_CLIPPING_Y);
            
                    psMtxEncContext->ui32SPEMvdClipRange = F_ENCODE(0, TOPAZHP_CR_SPE_MVD_CLIP_ENABLE);
                    psMtxEncContext->ui32JMCompControl = F_ENCODE((ctx->eStandard == IMG_STANDARD_MPEG4 ? 1 : 0), TOPAZHP_CR_JMCOMP_AC_ENABLE);
                }
                break;
            case IMG_STANDARD_JPEG:
			case IMG_STANDARD_NONE:
			default:
			    break;
        }
#ifdef FIRMWARE_BIAS
    //copy the bias tables
    {
        int n;
        for (n = 52; n >= 0; n -= 2)    {
            psMtxEncContext->aui32DirectBias_P[n >> 1] = ctx->sBiasTables.aui32DirectBias_P[n];
            psMtxEncContext->aui32InterBias_P[n >> 1] = ctx->sBiasTables.aui32InterBias_P[n];
            psMtxEncContext->aui32DirectBias_B[n >> 1] = ctx->sBiasTables.aui32DirectBias_B[n];
            psMtxEncContext->aui32InterBias_B[n >> 1] = ctx->sBiasTables.aui32InterBias_B[n];
        }
    }
#endif

    //copy the scale-tables
    ptg__generate_scale_tables(psMtxEncContext);
//      memcpy(psMtxEncContext->ui32InterIntraScale, ctx->ui32InterIntraScale, sizeof(IMG_UINT32)*SCALE_TBL_SZ);
//      memcpy(psMtxEncContext->ui32SkippedCodedScale, ctx->ui32SkippedCodedScale, sizeof(IMG_UINT32)*SCALE_TBL_SZ);

    // WEIGHTED PREDICTION
    psMtxEncContext->b8WeightedPredictionEnabled = ctx->bWeightedPrediction;
    psMtxEncContext->ui8MTXWeightedImplicitBiPred = ctx->ui8VPWeightedImplicitBiPred;

    // SEI_INSERTION
    psMtxEncContext->b8InsertHRDparams = ctx->bInsertHRDParams;
    if (psMtxEncContext->b8InsertHRDparams & !ctx->sRCParams.ui32BitsPerSecond) {   //ctx->uBitRate
        /* HRD parameters are meaningless without a bitrate */
        psMtxEncContext->b8InsertHRDparams = IMG_FALSE;
    }
    if (psMtxEncContext->b8InsertHRDparams) {
        psMtxEncContext->ui64ClockDivBitrate = (90000 * 0x100000000LL);
        psMtxEncContext->ui64ClockDivBitrate /= ctx->sRCParams.ui32BitsPerSecond;                       //ctx->uBitRate;
        psMtxEncContext->ui32MaxBufferMultClockDivBitrate = (IMG_UINT32)(((IMG_UINT64)(ctx->sRCParams.ui32BufferSize) *
                (IMG_UINT64) 90000) / (IMG_UINT64) ctx->sRCParams.ui32BitsPerSecond);
    }

    if (ctx->sRCParams.eRCMode != IMG_RCMODE_LLRC) {
        for (i = 0; i < MAX_CODED_BUFFERS; i++) {
            psMtxEncContext->aui32BytesCodedAddr[i] = 0;
        }
    }
    memcpy(&psMtxEncContext->sInParams, &ctx->sPicParams.sInParams, sizeof(IN_RC_PARAMS));
    // Update MV Scaling settings
    // IDR
    //      memcpy(&psMtxEncContext->sMVSettingsIdr, &psVideo->sMVSettingsIdr, sizeof(IMG_MV_SETTINGS));
    // NonB (I or P)
    //      for (i = 0; i <= MAX_BFRAMES; i++)
    //              memcpy(&psMtxEncContext->sMVSettingsNonB[i], &psVideo->sMVSettingsNonB[i], sizeof(IMG_MV_SETTINGS));

    psMtxEncContext->ui32LRITC_Cache_Chunk_Config =
        F_ENCODE(ctx->uChunksPerMb, TOPAZHP_CR_CACHE_CHUNKS_PER_MB) |
        F_ENCODE(ctx->uMaxChunks, TOPAZHP_CR_CACHE_CHUNKS_MAX) |
        F_ENCODE(ctx->uMaxChunks - ctx->uPriorityChunks, TOPAZHP_CR_CACHE_CHUNKS_PRIORITY);


    //they would be set in function ptg__prepare_templates()
    psMtxEncContext->ui32FirstPicFlags = ctx->ui32FirstPicFlags;
    psMtxEncContext->ui32NonFirstPicFlags = ctx->ui32NonFirstPicFlags;

#ifdef LTREFHEADER
    psMtxEncContext->i8SliceHeaderSlotNum = -1;
#endif
    {
        memset(psMtxEncContext->aui8PicOnLevel, 0, sizeof(psMtxEncContext->aui8PicOnLevel));
#ifdef _TOPAZHP_VIR_ADDR_
        psb_buffer_map(&(ps_mem->bufs_flat_gop), &(ps_mem->bufs_flat_gop.virtual_addr));
#endif
        ptg__minigop_generate_flat(ps_mem->bufs_flat_gop.virtual_addr, psMtxEncContext->ui32BFrameCount,
            psMtxEncContext->ui8RefSpacing, psMtxEncContext->aui8PicOnLevel);
#ifdef _TOPAZHP_VIR_ADDR_
        psb_buffer_unmap(&(ps_mem->bufs_flat_gop));
#endif
        
        if (ctx->sRCParams.b16Hierarchical) {
            memset(psMtxEncContext->aui8PicOnLevel, 0, sizeof(psMtxEncContext->aui8PicOnLevel));
#ifdef _TOPAZHP_VIR_ADDR_
            psb_buffer_map(&(ps_mem->bufs_hierar_gop), &(ps_mem->bufs_hierar_gop.virtual_addr));
#endif
            ptg_minigop_generate_hierarchical(ps_mem->bufs_hierar_gop.virtual_addr, psMtxEncContext->ui32BFrameCount,
                psMtxEncContext->ui8RefSpacing, psMtxEncContext->aui8PicOnLevel);
#ifdef _TOPAZHP_VIR_ADDR_
            psb_buffer_unmap(&(ps_mem->bufs_hierar_gop));
#endif

        }
    }

#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_unmap(&(ps_mem->bufs_mtx_context));
#endif

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s end\n", __FUNCTION__);

    return ;
}

static void ptg__setvideo_cmdbuf(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
    context_ENC_mem_size *ps_mem_size = &(ctx->ctx_mem_size);
    IMG_MTX_VIDEO_CONTEXT* psMtxEncContext = NULL;
    context_ENC_frame_buf *ps_buf = &(ctx->ctx_frame_buf);
    object_surface_p rec_surface = ps_buf->rec_surface;

#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_map(&(ps_mem->bufs_mtx_context), &(ps_mem->bufs_mtx_context.virtual_addr));
#endif
    psMtxEncContext = (IMG_MTX_VIDEO_CONTEXT*)(ps_mem->bufs_mtx_context.virtual_addr);
    IMG_INT i;

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);

    ptg_cmdbuf_set_phys(&(psMtxEncContext->ui32MVSettingsBTable), 0, 
        &(ps_mem->bufs_mv_setting_btable), 0, 0);
    if (ctx->sRCParams.b16Hierarchical)
        ptg_cmdbuf_set_phys(&psMtxEncContext->ui32MVSettingsHierarchical, 0, 
            &(ps_mem->bufs_mv_setting_hierar), 0, 0);

#if 0
    ptg_cmdbuf_set_phys(psMtxEncContext->apReconstructured, 0,
        &(rec_surface->psb_surface->buf), rec_surface->psb_surface->buf.buffer_ofs, 0);
drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s: rec_surface->psb_surface->buf.buffer_ofs = %d\n", __FUNCTION__, rec_surface->psb_surface->buf.buffer_ofs);

    {
        int i = 0;
        for (i = 1; i < ctx->i32PicNodes; i++)
            psMtxEncContext->apReconstructured[i] = psMtxEncContext->apReconstructured[0];
    }
//    ptg_cmdbuf_set_phys(&(psMtxEncContext->apReconstructured[1]), ctx->i32PicNodes - 1,
//        &(ps_mem->bufs_recon_pictures), 0, ps_mem_size->recon_pictures);
//
#else
    ptg_cmdbuf_set_phys(psMtxEncContext->apReconstructured, ctx->i32PicNodes,
        &(ps_mem->bufs_recon_pictures), 0, ps_mem_size->recon_pictures);
#endif
    ptg_cmdbuf_set_phys(psMtxEncContext->apColocated, ctx->i32PicNodes,
        &(ps_mem->bufs_colocated), 0, ps_mem_size->colocated);
    
    ptg_cmdbuf_set_phys(psMtxEncContext->apMV, ctx->i32MVStores,
        &(ps_mem->bufs_mv), 0, ps_mem_size->mv);

    if (ctx->bEnableMVC) {
        ptg_cmdbuf_set_phys(psMtxEncContext->apInterViewMV, 2, 
            &(ps_mem->bufs_interview_mv), 0, ps_mem_size->interview_mv);
    }

    ptg_cmdbuf_set_phys(psMtxEncContext->apWritebackRegions, WB_FIFO_SIZE,
        &(ctx->bufs_writeback), 0, ps_mem_size->writeback);

    ptg_cmdbuf_set_phys(psMtxEncContext->apAboveParams, ctx->i32NumPipes,
        &(ps_mem->bufs_above_params), 0, ps_mem_size->above_params);

    // SEI_INSERTION
    if (ctx->bInsertHRDParams) {
        ptg_cmdbuf_set_phys(&psMtxEncContext->pSEIBufferingPeriodTemplate, 0,
            &(ps_mem->bufs_sei_header), ps_mem_size->sei_header, ps_mem_size->sei_header);
        ptg_cmdbuf_set_phys(&psMtxEncContext->pSEIPictureTimingTemplate, 0,
            &(ps_mem->bufs_sei_header), ps_mem_size->sei_header * 2, ps_mem_size->sei_header);
    }

    ptg_cmdbuf_set_phys(psMtxEncContext->apSliceParamsTemplates, NUM_SLICE_TYPES,
        &(ps_mem->bufs_slice_template), 0, ps_mem_size->slice_template);
    
    ptg_cmdbuf_set_phys(psMtxEncContext->aui32SliceMap, ctx->ui8SlotsInUse,
        &(ps_mem->bufs_slice_map), 0, ps_mem_size->slice_map);

    // WEIGHTED PREDICTION
    if (ctx->bWeightedPrediction || (ctx->ui8VPWeightedImplicitBiPred == WBI_EXPLICIT)) {
        ptg_cmdbuf_set_phys(psMtxEncContext->aui32WeightedPredictionVirtAddr, ctx->ui8SlotsInUse,
            &(ps_mem->bufs_weighted_prediction), 0, ps_mem_size->weighted_prediction);
    }

    ptg_cmdbuf_set_phys(&psMtxEncContext->ui32FlatGopStruct, 0, &(ps_mem->bufs_flat_gop), 0, 0);
    if (psMtxEncContext->b8Hierarchical)
        ptg_cmdbuf_set_phys(&psMtxEncContext->ui32HierarGopStruct, 0, &(ps_mem->bufs_hierar_gop), 0, 0);

#ifdef LTREFHEADER
    ptg_cmdbuf_set_phys(psMtxEncContext->aui32LTRefHeader, ctx->ui8SlotsInUse,
        &(ps_mem->bufs_lt_ref_header), 0, ps_mem_size->lt_ref_header);
#endif

    if (ctx->sRCParams.eRCMode == IMG_RCMODE_LLRC) {
        for (i = 0; i < MAX_CODED_BUFFERS; i++) {
            //FIXME
            //ptg_cmdbuf_set_phys(&psMtxEncContext->aui32BytesCodedAddr, 0, 0);
            psMtxEncContext->aui32BytesCodedAddr[i] = 0;
        }
    }

    ptg_cmdbuf_set_phys(psMtxEncContext->apPicHdrTemplates, 4,
        &(ps_mem->bufs_pic_template), 0, ps_mem_size->pic_template);

    if (ctx->eStandard == IMG_STANDARD_H264) {
        ptg_cmdbuf_set_phys(&(psMtxEncContext->apSeqHeader), 0, 
            &(ps_mem->bufs_seq_header), 0, ps_mem_size->seq_header);
        if (ctx->bEnableMVC)
            ptg_cmdbuf_set_phys(&(psMtxEncContext->apSubSetSeqHeader), 0,
                &(ps_mem->bufs_sub_seq_header), 0, ps_mem_size->seq_header);
    }

#ifdef _TOPAZHP_OLD_LIBVA_
    for (i = 0; i < MAX_SOURCE_SLOTS_SL; i++) {
        psMtxEncContext->pFirstPassOutParamAddr[i] = 0;
#ifndef EXCLUDE_BEST_MP_DECISION_DATA
        psMtxEncContext->pFirstPassOutBestMultipassParamAddr[i] = 0;
#endif
    }
#else
    // Store the feedback memory address for all "5" slots in the context
    if (ctx->ui8EnableSelStatsFlags & ESF_FIRST_STAGE_STATS) {
        for (i = 0; i < ctx->ui8SlotsInUse; i++) {
            psMtxEncContext->pFirstPassOutParamAddr[i] = 0;
        }
    }
#ifndef EXCLUDE_BEST_MP_DECISION_DATA
    // Store the feedback memory address for all "5" slots in the context
    if (ctx->ui8EnableSelStatsFlags & ESF_MP_BEST_MB_DECISION_STATS
        || ctx->ui8EnableSelStatsFlags & ESF_MP_BEST_MOTION_VECTOR_STATS) {
        for (i = 0; i < ctx->ui8SlotsInUse; i++) {
            psMtxEncContext->pFirstPassOutBestMultipassParamAddr[i] = 0;
        }
    }
#endif
#endif

    //Store the MB-Input control parameter memory for all the 5-slots in the context
    if (ctx->bEnableInpCtrl) {
        ptg_cmdbuf_set_phys(psMtxEncContext->pMBCtrlInParamsAddr, ctx->ui8SlotsInUse,
            &(ps_mem->bufs_mb_ctrl_in_params), 0, ps_mem_size->mb_ctrl_in_params);
    }

#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_unmap(&(ps_mem->bufs_mtx_context));
#endif

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s end\n", __FUNCTION__);

    return ;
}

static VAStatus ptg__validate_params(context_ENC_p ctx)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    if (ctx->eStandard == IMG_STANDARD_H264) {
        if ((ctx->ui8DeblockIDC == 0) && (ctx->bArbitrarySO))
            ctx->ui8DeblockIDC = 2;

        if ((ctx->ui8DeblockIDC == 0) && (ctx->i32NumPipes > 1) && (ctx->ui8SlicesPerPicture > 1)) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "WARNING: Full deblocking with multiple pipes will cause a mismatch between reconstructed and encoded video\n");
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "Consider using -deblockIDC 2 or -deblockIDC 1 instead if matching reconstructed video is required.\n");
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "WARNING: Forcing -deblockIDC = 2 for HW verification.\n");
            ctx->ui8DeblockIDC = 2;
        }
    } else if (ctx->eStandard == IMG_STANDARD_H263) {
		ctx->bArbitrarySO = IMG_FALSE;
		ctx->ui8DeblockIDC = 1;
    } else {
        ctx->ui8DeblockIDC = 1;
    }

#ifdef _TOPAZHP_OLD_LIBVA_
    ctx->sRCParams.ui32SliceByteLimit = 0;
    ctx->sRCParams.ui32SliceMBLimit = 0;
#else
    ctx->sRCParams.ui32SliceByteLimit = ctx->sCapsParams.ui16MaxSlices;
    ctx->sRCParams.ui32SliceMBLimit = ctx->sCapsParams.ui16MaxSlices >> 4;
#endif
    //slice params
    if (ctx->ui8SlicesPerPicture == 0)
        ctx->ui8SlicesPerPicture = ctx->sCapsParams.ui16RecommendedSlices;
    else {
        if (ctx->ui8SlicesPerPicture > ctx->sCapsParams.ui16MaxSlices)
            ctx->ui8SlicesPerPicture = ctx->sCapsParams.ui16MaxSlices;
        else if (ctx->ui8SlicesPerPicture < ctx->sCapsParams.ui16MinSlices)
            ctx->ui8SlicesPerPicture = ctx->sCapsParams.ui16MinSlices;
    }

    return vaStatus;
}

static VAStatus ptg__validate_busize(context_ENC_p ctx)
{
    //IMG_RC_PARAMS *psRCParams = &(ctx->sRCParams);
    // if no BU size is given then pick one ourselves, if doing arbitrary slice order then make BU = width in bu's
    // forces slice boundaries to no be mid-row
    if (ctx->bArbitrarySO || (ctx->ui32BasicUnit == 0)) {
        ctx->ui32BasicUnit = (ctx->ui16Width / 16);
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "Patched Basic unit to %d\n", ctx->ui32BasicUnit);
    } else {
        IMG_UINT32 ui32MBs, ui32MBsperSlice, ui32MBsLastSlice;
        IMG_UINT32 ui32BUs;
        IMG_INT32  i32SliceHeight;
        IMG_UINT32 ui32MaxSlicesPerPipe, ui32MaxMBsPerPipe, ui32MaxBUsPerPipe;

        ui32MBs  = ctx->ui16PictureHeight * ctx->ui16Width / (16 * 16);

        i32SliceHeight = ctx->ui16PictureHeight / ctx->ui8SlicesPerPicture;
        i32SliceHeight &= ~15;

        ui32MBsperSlice = (i32SliceHeight * ctx->ui16Width) / (16 * 16);
        ui32MBsLastSlice = ui32MBs - (ui32MBsperSlice * (ctx->ui8SlicesPerPicture - 1));

        // they have given us a basic unit so validate it
        if (ctx->ui32BasicUnit < 6) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "\nERROR: Basic unit size too small, must be greater than 6\n");
            return VA_STATUS_ERROR_UNKNOWN;
        }

        if (ctx->ui32BasicUnit > ui32MBsperSlice) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "\nERROR: Basic unit size(%d) too large", ctx->ui32BasicUnit);
            drv_debug_msg(VIDEO_DEBUG_GENERAL, " must not be greater than the number of macroblocks in a slice (%d)\n", ui32MBsperSlice);
            return VA_STATUS_ERROR_UNKNOWN;
        }
        if (ctx->ui32BasicUnit > ui32MBsLastSlice) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "\nERROR: Basic unit size(%d) too large", ctx->ui32BasicUnit);
            drv_debug_msg(VIDEO_DEBUG_GENERAL, " must not be greater than the number of macroblocks in a slice (%d)\n", ui32MBsLastSlice);
            return VA_STATUS_ERROR_UNKNOWN;
        }

        ui32BUs = ui32MBsperSlice / ctx->ui32BasicUnit;
        if ((ui32BUs * ctx->ui32BasicUnit) != ui32MBsperSlice) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "\nERROR: Basic unit size(%d) not an integer divisor of MB's in a slice(%d)",
                                     ctx->ui32BasicUnit, ui32MBsperSlice);
            return VA_STATUS_ERROR_UNKNOWN;
        }

        ui32BUs = ui32MBsLastSlice / ctx->ui32BasicUnit;
        if ((ui32BUs * ctx->ui32BasicUnit) != ui32MBsLastSlice) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "\nERROR: Basic unit size(%d) not an integer divisor of MB's in the last slice(%d)",
                                     ctx->ui32BasicUnit, ui32MBsLastSlice);
            return VA_STATUS_ERROR_UNKNOWN;
        }

        // check if the number of BUs per pipe is greater than 200
        ui32MaxSlicesPerPipe = (ctx->ui8SlicesPerPicture + ctx->i32NumPipes - 1) / ctx->i32NumPipes;
        ui32MaxMBsPerPipe = (ui32MBsperSlice * (ui32MaxSlicesPerPipe - 1)) + ui32MBsLastSlice;
        ui32MaxBUsPerPipe = (ui32MaxMBsPerPipe + ctx->ui32BasicUnit - 1) / ctx->ui32BasicUnit;
        if (ui32MaxBUsPerPipe > 200) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "\nERROR: Basic unit size too small. There must be less than 201 basic units per slice");
            return VA_STATUS_ERROR_UNKNOWN;
        }
    }

    ctx->sRCParams.ui32BUSize = ctx->ui32BasicUnit;
    return VA_STATUS_SUCCESS;
}

#ifdef _TOPAZHP_PDUMP_
static void ptg__trace_cmdbuf(ptg_cmdbuf_p cmdbuf, int idx)
{
    IMG_UINT32 ui32CmdTmp[4];
    IMG_UINT32 *ptmp = (IMG_UINT32 *)(cmdbuf->cmd_start);
    IMG_UINT32 *pend = (IMG_UINT32 *)(cmdbuf->cmd_idx);
    IMG_UINT32 ui32Len;

#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: start, stream (%d)\n", __FUNCTION__, idx);
#endif

    if(idx == 0) {
        //skip the newcodec
        if (*ptmp != MTX_CMDID_SW_NEW_CODEC) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: error new coded\n", __FUNCTION__);
            return ;
        }
        ptmp += 3;
        //skip the write register
        if (*ptmp++ != MTX_CMDID_SW_WRITEREG) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: error writereg = 0x%08x\n", __FUNCTION__, *(ptmp-1));
            return ;
         }
         ui32Len = *ptmp++;
         ptmp += (ui32Len * 3);

         drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: Len(%d), cmd = 0x%08x\n", __FUNCTION__, (int)ui32Len, (unsigned int)(*ptmp));
    }

    while ((*ptmp&0xf) == MTX_CMDID_SETVIDEO ) {

        ui32CmdTmp[0] = *ptmp++;
        ui32CmdTmp[1] = *ptmp++;
        ui32CmdTmp[2] = *ptmp++;
        ui32CmdTmp[3] = 0;

        topazhp_dump_command((unsigned int*)ui32CmdTmp);
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: output cmd 0x%08x\n", __FUNCTION__, (int)(ui32CmdTmp[0]));
    }

    do{
        ui32CmdTmp[0] = *ptmp++;
        ui32CmdTmp[1] = *ptmp++;
        ui32CmdTmp[2] = 0;
        ui32CmdTmp[3] = 0;
        topazhp_dump_command((unsigned int*)ui32CmdTmp);
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: output cmd 0x%08x\n", __FUNCTION__, (int)(ui32CmdTmp[0]));
    }while (!(((ui32CmdTmp[0]&0xf) == MTX_CMDID_ENCODE_FRAME) ||
            ((ui32CmdTmp[0]&0xf) == MTX_CMDID_SETVIDEO)));

    while ((*ptmp&0xf) == MTX_CMDID_SETVIDEO ) {

        ui32CmdTmp[0] = *ptmp++;
        ui32CmdTmp[1] = *ptmp++;
        ui32CmdTmp[2] = *ptmp++;
        ui32CmdTmp[3] = 0;

        topazhp_dump_command((unsigned int*)ui32CmdTmp);
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: output cmd 0x%08x\n", __FUNCTION__, (int)(ui32CmdTmp[0]));
    }

#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: end\n", __FUNCTION__);
#endif

    return ;
}
#endif

/***********************************************************************************
 * Function Name      : APP_FillSliceMap
 * Inputs             : psContext
 * Outputs            :
 * Returns            :
 * Description        : Fills slice map for a given source picture
 ************************************************************************************/
IMG_UINT16 APP_Rand(context_ENC_p ctx) 
{
  IMG_UINT16 ui16ret = 0;
/*
  psContext->ui32pseudo_rand_seed =  (IMG_UINT32) ((psContext->ui32pseudo_rand_seed * 1103515245 + 12345) & 0xffffffff); //Using mask, just in case
  ui16ret = (IMG_UINT16)(psContext->ui32pseudo_rand_seed / 65536) % 32768; 
*/
  return ui16ret;
}


static IMG_UINT32 ptg__fill_slice_map(context_ENC_p ctx, IMG_INT32 i32SlotNum, IMG_UINT32 ui32StreamIndex)
{
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
    unsigned char *pvBuffer;
    IMG_UINT8 ui8SlicesPerPicture;
    IMG_UINT8 ui8HalfWaySlice;
    IMG_UINT32 ui32HalfwayBU;

    ui8SlicesPerPicture = ctx->ui8SlicesPerPicture;
    ui32HalfwayBU = 0;
    ui8HalfWaySlice=ui8SlicesPerPicture/2;
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: slot num = %d, aso = %d\n", __FUNCTION__, i32SlotNum, ctx->bArbitrarySO);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: stream id = %d, addr = 0x%x\n", __FUNCTION__, ui32StreamIndex, ps_mem->bufs_slice_map.virtual_addr);
#endif

#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_map(&(ps_mem->bufs_slice_map), &(ps_mem->bufs_slice_map.virtual_addr));
#endif
    pvBuffer = (unsigned char*)(ps_mem->bufs_slice_map.virtual_addr + (i32SlotNum * ctx->ctx_mem_size.slice_map));

    if (ctx->bArbitrarySO) {
        IMG_UINT8 ui8Index;
        IMG_UINT8 ui32FirstBUInSlice;
        IMG_UINT8 ui8SizeInKicks;
        IMG_UINT8 ui8TotalBUs;
        IMG_UINT8 aui8SliceNumbers[MAX_SLICESPERPIC];

        ui8SlicesPerPicture = APP_Rand(ctx) % ctx->ui8SlicesPerPicture + 1;
        // Fill slice map
        // Fill number of slices
        * pvBuffer = ui8SlicesPerPicture;
        pvBuffer++;

        for (ui8Index = 0; ui8Index < ui8SlicesPerPicture; ui8Index++)
            aui8SliceNumbers[ui8Index] = ui8Index;

	// randomise slice numbers
        for (ui8Index = 0; ui8Index < 20; ui8Index++) {
            IMG_UINT8 ui8FirstCandidate;
            IMG_UINT8 ui8SecondCandidate;
            IMG_UINT8 ui8Temp;

            ui8FirstCandidate = APP_Rand(ctx) % ui8SlicesPerPicture;
            ui8SecondCandidate = APP_Rand(ctx) % ui8SlicesPerPicture;

            ui8Temp = aui8SliceNumbers[ui8FirstCandidate];
            aui8SliceNumbers[ui8FirstCandidate] = aui8SliceNumbers[ui8SecondCandidate];
            aui8SliceNumbers[ui8SecondCandidate] = ui8Temp;
        }

        ui8TotalBUs = (ctx->ui16PictureHeight / 16) * (ctx->ui16Width / 16) / ctx->sRCParams.ui32BUSize;

        ui32FirstBUInSlice = 0;

        for (ui8Index = 0; ui8Index < ui8SlicesPerPicture - 1; ui8Index++) {
            IMG_UINT32 ui32BUsCalc;
            if (ui8Index==ui8HalfWaySlice) ui32HalfwayBU=ui32FirstBUInSlice;

            ui32BUsCalc=(ui8TotalBUs - 1 * (ui8SlicesPerPicture - ui8Index));
            if(ui32BUsCalc)
                ui8SizeInKicks = APP_Rand(ctx) %ui32BUsCalc  + 1;
            else
                ui8SizeInKicks = 1;
            ui8TotalBUs -= ui8SizeInKicks;

            // slice number
            * pvBuffer = aui8SliceNumbers[ui8Index];
            pvBuffer++;

            // SizeInKicks BU
            * pvBuffer = ui8SizeInKicks;
            pvBuffer++;
            ui32FirstBUInSlice += (IMG_UINT32) ui8SizeInKicks;
        }
        ui8SizeInKicks = ui8TotalBUs;
        // slice number
        * pvBuffer = aui8SliceNumbers[ui8SlicesPerPicture - 1];
        pvBuffer++;

        // last BU
        * pvBuffer = ui8SizeInKicks;
        pvBuffer++;
    } else {
        // Fill standard Slice Map (non arbitrary)
        IMG_UINT8 ui8Index;
        IMG_UINT8 ui8SliceNumber;
        IMG_UINT8 ui32FirstBUInSlice;
        IMG_UINT8 ui8SizeInKicks;
        IMG_UINT32 ui32SliceHeight;

        // Fill number of slices
        * pvBuffer = ui8SlicesPerPicture;
        pvBuffer++;


        ui32SliceHeight = (ctx->ui16PictureHeight / ctx->ui8SlicesPerPicture) & ~15;

        ui32FirstBUInSlice = 0;
        ui8SliceNumber = 0;
        for (ui8Index = 0; ui8Index < ui8SlicesPerPicture - 1; ui8Index++) {
            if (ui8Index==ui8HalfWaySlice) ui32HalfwayBU=ui32FirstBUInSlice;
            ui8SizeInKicks = ((ui32SliceHeight / 16)*(ctx->ui16Width/16))/ctx->sRCParams.ui32BUSize;

            // slice number
            * pvBuffer = ui8SliceNumber;
            pvBuffer++;
            // SizeInKicks BU
            * pvBuffer = ui8SizeInKicks;
            pvBuffer++;

            ui8SliceNumber++;
            ui32FirstBUInSlice += (IMG_UINT32) ui8SizeInKicks;
        }
        ui32SliceHeight = ctx->ui16PictureHeight - ui32SliceHeight * (ctx->ui8SlicesPerPicture - 1);
        if (ui8Index==ui8HalfWaySlice) ui32HalfwayBU=ui32FirstBUInSlice;
            ui8SizeInKicks = ((ui32SliceHeight / 16)*(ctx->ui16Width/16))/ctx->sRCParams.ui32BUSize;

            // slice number
            * pvBuffer = ui8SliceNumber;
            pvBuffer++;
            // last BU
            * pvBuffer = ui8SizeInKicks;
            pvBuffer++;
	}
#ifdef _TOPAZHP_VIR_ADDR_
        psb_buffer_unmap(&(ps_mem->bufs_slice_map));
#endif

    return ui32HalfwayBU;
}

void ptg__trace_seq_header(context_ENC_p ctx)
{
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: ucProfile = %d\n",              __FUNCTION__, ctx->ui8ProfileIdc);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: ucLevel = %d\n",                __FUNCTION__, ctx->ui8LevelIdc);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: ui16Width = %d\n",              __FUNCTION__, ctx->ui16Width);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: ui16PictureHeight = %d\n",      __FUNCTION__, ctx->ui16PictureHeight);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: ui32CustomQuantMask = %d\n",    __FUNCTION__, ctx->ui32CustomQuantMask);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: ui8FieldCount = %d\n",          __FUNCTION__, ctx->ui8FieldCount);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: ui8MaxNumRefFrames = %d\n",     __FUNCTION__, ctx->ui8MaxNumRefFrames);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: bPpsScaling = %d\n",            __FUNCTION__, ctx->bPpsScaling);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: bUseDefaultScalingList = %d\n", __FUNCTION__, ctx->bUseDefaultScalingList);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: bEnableLossless = %d\n",        __FUNCTION__, ctx->bEnableLossless);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: bArbitrarySO = %d\n",           __FUNCTION__, ctx->bArbitrarySO);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: vui_flag = %d\n",               __FUNCTION__, ctx->sVuiParams.vui_flag);
}

static VAStatus ptg__H264ES_validate_params(context_ENC_p ctx)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    
    if ((ctx->ui8DeblockIDC == 0) && (ctx->bArbitrarySO))
        ctx->ui8DeblockIDC = 2;
    
    if ((ctx->ui8DeblockIDC == 0) && (ctx->i32NumPipes > 1)) {
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "WARNING: Full deblocking with multiple pipes will cause a mismatch between reconstructed and encoded video\n");
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "Consider using -deblockIDC 2 or -deblockIDC 1 instead if matching reconstructed video is required.\n");
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "WARNING: Forcing -deblockIDC = 2 for HW verification.\n");
        ctx->ui8DeblockIDC = 2;
    }
    return vaStatus;
}

static void ptg__H264ES_send_seq_header(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;
    IMG_RC_PARAMS *psRCParams = &(ctx->sRCParams);
    H264_VUI_PARAMS *psVuiParams = &(ctx->sVuiParams);
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);
#endif
    ptg__H264ES_validate_params(ctx);

    memset(psVuiParams, 0, sizeof(H264_VUI_PARAMS));

    if (psRCParams->eRCMode != IMG_RCMODE_NONE) {
        psVuiParams->vui_flag = 1;
        psVuiParams->Time_Scale = psRCParams->ui32FrameRate * 2;
        psVuiParams->bit_rate_value_minus1 = psRCParams->ui32BitsPerSecond / 64 - 1;
        psVuiParams->cbp_size_value_minus1 = psRCParams->ui32BufferSize / 64 - 1;
        psVuiParams->CBR = ((psRCParams->eRCMode == IMG_RCMODE_CBR) && (!psRCParams->bDisableBitStuffing))?1:0;
        psVuiParams->initial_cpb_removal_delay_length_minus1 = BPH_SEI_NAL_INITIAL_CPB_REMOVAL_DELAY_SIZE - 1;
        psVuiParams->cpb_removal_delay_length_minus1 = PTH_SEI_NAL_CPB_REMOVAL_DELAY_SIZE - 1;
        psVuiParams->dpb_output_delay_length_minus1 = PTH_SEI_NAL_DPB_OUTPUT_DELAY_SIZE - 1;
        psVuiParams->time_offset_length = 24;
    }
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s psVuiParams->vui_flag = %d\n", __FUNCTION__, psVuiParams->vui_flag);
#endif

#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_map(&(ps_mem->bufs_seq_header), &(ps_mem->bufs_seq_header.virtual_addr));
#endif

    ptg__H264ES_prepare_sequence_header(
        ps_mem->bufs_seq_header.virtual_addr,
        &(ctx->sVuiParams),
        &(ctx->sCropParams),
        ctx->ui16Width,         //ui8_picture_width_in_mbs
        ctx->ui16PictureHeight, //ui8_picture_height_in_mbs
        ctx->ui32CustomQuantMask,    //0,  ui8_custom_quant_mask
        ctx->ui8ProfileIdc,          //ui8_profile
        ctx->ui8LevelIdc,            //ui8_level
        ctx->ui8FieldCount,          //1,  ui8_field_count
        ctx->ui8MaxNumRefFrames,     //1,  ui8_max_num_ref_frames
        ctx->bPpsScaling,            //0   ui8_pps_scaling_cnt
        ctx->bUseDefaultScalingList, //0,  b_use_default_scaling_list
        ctx->bEnableLossless,        //0,  blossless
        ctx->bArbitrarySO
    );
#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_unmap(&(ps_mem->bufs_seq_header));
#endif

    if (ctx->bEnableMVC) {
#ifdef _TOPAZHP_VIR_ADDR_
        psb_buffer_map(&(ps_mem->bufs_sub_seq_header), &(ps_mem->bufs_sub_seq_header.virtual_addr));
#endif
        ptg__H264ES_prepare_mvc_sequence_header(
            ps_mem->bufs_sub_seq_header.virtual_addr,
            &(ctx->sCropParams),
            ctx->ui16Width,         //ui8_picture_width_in_mbs
            ctx->ui16PictureHeight, //ui8_picture_height_in_mbs
            ctx->ui32CustomQuantMask,    //0,  ui8_custom_quant_mask
            ctx->ui8ProfileIdc,          //ui8_profile
            ctx->ui8LevelIdc,            //ui8_level
            ctx->ui8FieldCount,          //1,  ui8_field_count
            ctx->ui8MaxNumRefFrames,     //1,  ui8_max_num_ref_frames
            ctx->bPpsScaling,            //0   ui8_pps_scaling_cnt
            ctx->bUseDefaultScalingList, //0,  b_use_default_scaling_list
            ctx->bEnableLossless,        //0,  blossless
            ctx->bArbitrarySO
        );
#ifdef _TOPAZHP_VIR_ADDR_
        psb_buffer_unmap(&(ps_mem->bufs_sub_seq_header));
#endif
    }

    cmdbuf->cmd_idx_saved[PTG_CMDBUF_SEQ_HEADER_IDX] = cmdbuf->cmd_idx;
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s end\n", __FUNCTION__);
#endif
}

static void ptg__H264ES_send_pic_header(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
    IMG_BOOL bDepViewPPS = IMG_FALSE;

    if ((ctx->bEnableMVC) && (ctx->ui16MVCViewIdx != 0) &&
        (ctx->ui16MVCViewIdx != (IMG_UINT16)(NON_MVC_VIEW))) {
        bDepViewPPS = IMG_TRUE;
    }

#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_map(&(ps_mem->bufs_pic_template), &(ps_mem->bufs_pic_template.virtual_addr));
#endif

    ptg__H264ES_prepare_picture_header(
        ps_mem->bufs_pic_template.virtual_addr,
        0, //IMG_BOOL    bCabacEnabled,
        ctx->bH2648x8Transform, //IMG_BOOL    b_8x8transform,
        0, //IMG_BOOL    bIntraConstrained,
        0, //IMG_INT8    i8CQPOffset,
        0, //IMG_BOOL    bWeightedPrediction,
        0, //IMG_UINT8   ui8WeightedBiPred,
        bDepViewPPS, //IMG_BOOL    bMvcPPS,
        0, //IMG_BOOL    bScalingMatrix,
        0  //IMG_BOOL    bScalingLists
    );
#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_unmap(&(ps_mem->bufs_pic_template));
#endif
}

static void ptg__H264ES_send_hrd_header(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
    H264_VUI_PARAMS *psVuiParams = &(ctx->sVuiParams);
    IMG_UINT8 aui8clocktimestampflag[1];
    aui8clocktimestampflag[0] = IMG_FALSE;
#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_map(&(ps_mem->bufs_sei_header), &(ps_mem->bufs_sei_header.virtual_addr));
#endif

    if ((!ctx->bEnableMVC) || (ctx->ui16MVCViewIdx == 0)) {
        ptg__H264ES_prepare_AUD_header(ps_mem->bufs_sei_header.virtual_addr);
    }
    
    ptg__H264ES_prepare_SEI_buffering_period_header(
        ps_mem->bufs_sei_header.virtual_addr + (ctx->ctx_mem_size.sei_header),
        0,// ui8cpb_cnt_minus1,
        psVuiParams->initial_cpb_removal_delay_length_minus1+1, //ui8initial_cpb_removal_delay_length,
        1, //ui8NalHrdBpPresentFlag,
        14609, // ui32nal_initial_cpb_removal_delay,
        62533, //ui32nal_initial_cpb_removal_delay_offset,
        0, //ui8VclHrdBpPresentFlag - CURRENTLY HARD CODED TO ZERO IN TOPAZ
        NOT_USED_BY_TOPAZ, // ui32vcl_initial_cpb_removal_delay, (not used when ui8VclHrdBpPresentFlag = 0)
        NOT_USED_BY_TOPAZ); // ui32vcl_initial_cpb_removal_delay_offset (not used when ui8VclHrdBpPresentFlag = 0)

    ptg__H264ES_prepare_SEI_picture_timing_header(
        ps_mem->bufs_sei_header.virtual_addr + (ctx->ctx_mem_size.sei_header * 2),
        1, //ui8CpbDpbDelaysPresentFlag,
        psVuiParams->cpb_removal_delay_length_minus1, //cpb_removal_delay_length_minus1,
        psVuiParams->dpb_output_delay_length_minus1, //dpb_output_delay_length_minus1,
        20, //ui32cpb_removal_delay,
        2, //ui32dpb_output_delay,
        0, //ui8pic_struct_present_flag (contained in the sequence header, Topaz hard-coded default to 0)
        NOT_USED_BY_TOPAZ, //ui8pic_struct, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //NumClockTS, (not used when ui8pic_struct_present_flag = 0)
        aui8clocktimestampflag, //abclock_timestamp_flag, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //ui8full_timestamp_flag, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //ui8seconds_flag, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //ui8minutes_flag, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //ui8hours_flag, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //seconds_value, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //minutes_value, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //hours_value, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //ct_type (2=Unknown) See TRM Table D 2 ?Mapping of ct_type to source picture scan  (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //nuit_field_based_flag, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //counting_type (See TRM Table D 3 ?Definition of counting_type values)  (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //ui8discontinuity_flag, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //ui8cnt_dropped_flag, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //n_frames, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ, //time_offset_length, (not used when ui8pic_struct_present_flag = 0)
        NOT_USED_BY_TOPAZ); //time_offset (not used when ui8pic_struct_present_flag = 0)
#ifdef _TOPAZHP_VIR_ADDR_
    psb_buffer_unmap(&(ps_mem->bufs_sei_header));
#endif

}


static VAStatus ptg__cmdbuf_new_codec(context_ENC_p ctx)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;
    psb_driver_data_p driver_data = ctx->obj_context->driver_data;
    
    *cmdbuf->cmd_idx++ =
        ((MTX_CMDID_SW_NEW_CODEC & MTX_CMDWORD_ID_MASK) << MTX_CMDWORD_ID_SHIFT) |
        (((driver_data->drm_context & MTX_CMDWORD_COUNT_MASK) << MTX_CMDWORD_COUNT_SHIFT));
    ptg_cmdbuf_insert_command_param(ctx->eCodec);
    ptg_cmdbuf_insert_command_param((ctx->ui16Width << 16) | ctx->ui16PictureHeight);

    return vaStatus;
}

#ifdef _TOPAZHP_DUAL_STREAM_
static VAStatus ptg__cmdbuf_lowpower(context_ENC_p ctx)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;
    psb_driver_data_p driver_data = ctx->obj_context->driver_data;
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[0]);
    context_ENC_mem_size *ps_size = &(ctx->ctx_mem_size);


    *cmdbuf->cmd_idx++ =
        ((MTX_CMDID_SW_LEAVE_LOWPOWER & MTX_CMDWORD_ID_MASK) << MTX_CMDWORD_ID_SHIFT) |
        (((driver_data->drm_context & MTX_CMDWORD_COUNT_MASK) << MTX_CMDWORD_COUNT_SHIFT));

#ifdef _TOPAZHP_OLD_LIBVA_
    ptg_cmdbuf_set_phys(cmdbuf->cmd_idx, 0,
        &(ps_mem->bufs_lowpower_reg), 0, 0);
	++(cmdbuf->cmd_idx);
    ptg_cmdbuf_set_phys(cmdbuf->cmd_idx, 0,
        &(ps_mem->bufs_lowpower_data), 0, 0);
	++(cmdbuf->cmd_idx);
    ptg_cmdbuf_insert_command_param(ctx->eCodec);
	++(cmdbuf->cmd_idx);
#else
    RELOC_PICMGMT_PARAMS_PTG(cmdbuf->cmd_idx,
        0, &(ps_mem->bufs_lowpower_reg));
    RELOC_PICMGMT_PARAMS_PTG(cmdbuf->cmd_idx,
        0, &(ps_mem->bufs_lowpower_data));
    ptg_cmdbuf_insert_command_param(ctx->eCodec);
#endif

    return vaStatus;
}
#endif

static VAStatus ptg__cmdbuf_load_bias(context_ENC_p ctx)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    
    //init bias parameters
     ptg_init_bias_params(ctx);
    
     vaStatus = ptg__generate_bias(ctx);
     if (vaStatus != VA_STATUS_SUCCESS) {
         drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: generate bias params\n", __FUNCTION__, vaStatus);
     }
    
     vaStatus = ptg_load_bias(ctx, IMG_INTER_P);
     if (vaStatus != VA_STATUS_SUCCESS) {
         drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: load bias params\n", __FUNCTION__, vaStatus);
     }
    return vaStatus;
}

static VAStatus ptg__cmdbuf_setvideo(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ui32StreamIndex]);
    
    ptg__setvideo_params(ctx, ui32StreamIndex);
    ptg__setvideo_cmdbuf(ctx, ui32StreamIndex);

    ptg_cmdbuf_insert_command_package(ctx->obj_context, ui32StreamIndex,
        MTX_CMDID_SETVIDEO, &(ps_mem->bufs_mtx_context), 0);

    return vaStatus;
}

static VAStatus ptg__cmdbuf_provide_buffer(context_ENC_p ctx, IMG_UINT32 ui32StreamIndex)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    
    if (ctx->sRCParams.ui16BFrames > 0)
        ptg__provide_buffer_BFrames(ctx, ui32StreamIndex);
    else
        ptg__provide_buffer_PFrames(ctx, ui32StreamIndex);
 
    return vaStatus;
}



VAStatus ptg_EndPicture(context_ENC_p ctx)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    IMG_INT32 i32Ret = 0;
    IMG_INT32 i;
    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;
    context_ENC_frame_buf *ps_buf = &(ctx->ctx_frame_buf);
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ctx->ui32StreamID]);
    unsigned char is_JPEG;

#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: ctx->ui8SlicesPerPicture = %d, ctx->ui32StreamID = %d\n", __FUNCTION__, ctx->ui8SlicesPerPicture, ctx->ui32StreamID);
#endif
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: ctx->ui32FrameCount[0] = %d, ctx->ui32FrameCount[1] = %d\n", __FUNCTION__, ctx->ui32FrameCount[0], ctx->ui32FrameCount[1]);

    if ((ctx->ui32FrameCount[0] == 0)&&(ctx->ui32StreamID == 0)) {
#ifdef _TOPAZHP_ALLOC_
            //cmdbuf setup
            ctx->ctx_cmdbuf[0].ui32LowCmdCount = 0xa5a5a5a5 %  MAX_TOPAZ_CMD_COUNT;
            ctx->ctx_cmdbuf[0].ui32HighCmdCount = 0;
            ctx->ctx_cmdbuf[0].ui32HighWBReceived = 0;

            is_JPEG = (ctx->eStandard == IMG_STANDARD_JPEG) ? 1 : 0;
            vaStatus = ptg__alloc_context_buffer(ctx, is_JPEG, 0);
            if (vaStatus != VA_STATUS_SUCCESS) {
                drv_debug_msg(VIDEO_DEBUG_ERROR, "setup enc profile");
            }
        
            if (ctx->bEnableMVC) {
                 //cmdbuf setup
                ctx->ctx_cmdbuf[1].ui32LowCmdCount = 0xa5a5a5a5 %  MAX_TOPAZ_CMD_COUNT;
                ctx->ctx_cmdbuf[1].ui32HighCmdCount = 0;
                ctx->ctx_cmdbuf[1].ui32HighWBReceived = 0;
                vaStatus = ptg__alloc_context_buffer(ctx, is_JPEG, 1);
                if (vaStatus != VA_STATUS_SUCCESS) {
                    drv_debug_msg(VIDEO_DEBUG_ERROR, "setup enc profile");
                }
            }
#endif

        
        vaStatus = ptg__validate_params(ctx);
        if (vaStatus != VA_STATUS_SUCCESS) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "validate params");
        }
        
        //FIXME: Zhaohan from DDK APP_InitAdaptiveRoundingTables();
        if (ctx->eStandard != IMG_STANDARD_H264)
            ctx->bVPAdaptiveRoundingDisable = IMG_TRUE;

        vaStatus = ptg__validate_busize(ctx);
        if (vaStatus != VA_STATUS_SUCCESS) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "validate busize");
        }

        vaStatus = ptg__cmdbuf_new_codec(ctx);
        if (vaStatus != VA_STATUS_SUCCESS) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "cmdbuf new codec\n");
        }
    }
#ifdef _TOPAZHP_DUAL_STREAM_
    vaStatus = ptg__cmdbuf_lowpower(ctx);
    if (vaStatus != VA_STATUS_SUCCESS) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "cmdbuf lowpower\n");
    }
#endif
    if (ctx->ui32FrameCount[0] == 0) {
        if (ctx->eStandard == IMG_STANDARD_H264) {
            ptg__H264ES_send_seq_header(ctx, 0);
            ptg__H264ES_send_pic_header(ctx, 0);
            if (ctx->bInsertHRDParams)
                ptg__H264ES_send_hrd_header(ctx, 0);
        }

        for (i = 0; i < ctx->ui8SlotsInUse; i++) {
            i32Ret = ptg__fill_slice_map(ctx, i, 0);
            if (i32Ret < 0) {
                drv_debug_msg(VIDEO_DEBUG_ERROR, "fill slice map\n");
            }
        }

        vaStatus = ptg__prepare_templates(ctx, 0);
        if (vaStatus != VA_STATUS_SUCCESS) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "prepare_templates\n");
        }

        vaStatus = ptg__cmdbuf_load_bias(ctx);
        if (vaStatus != VA_STATUS_SUCCESS) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "cmdbuf load bias\n");
        }

        vaStatus = ptg__cmdbuf_setvideo(ctx, 0);
        if (vaStatus != VA_STATUS_SUCCESS) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "cmdbuf setvideo\n");
        }
    }



    vaStatus = ptg__cmdbuf_provide_buffer(ctx, ctx->ui32StreamID);
    if (vaStatus != VA_STATUS_SUCCESS) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "provide buffer");
    }

    if ((ctx->ui32FrameCount[0] == 0) && (ctx->bEnableMVC)) {
        ptg__H264ES_send_seq_header(ctx, 1);
        ptg__H264ES_send_pic_header(ctx, 1);
        if (ctx->bInsertHRDParams)
            ptg__H264ES_send_hrd_header(ctx, 1);

        for (i = 0; i < ctx->ui8SlotsInUse; i++) {
            i32Ret = ptg__fill_slice_map(ctx, i, 1);
            if (i32Ret < 0) {
                 drv_debug_msg(VIDEO_DEBUG_ERROR, "fill slice map\n");
            }
        }

        vaStatus = ptg__prepare_templates(ctx, 1);
        if (vaStatus != VA_STATUS_SUCCESS) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "prepare_templates 1 \n");
        }

        vaStatus = ptg__cmdbuf_setvideo(ctx, 1);
        if (vaStatus != VA_STATUS_SUCCESS) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "cmdbuf setvideo\n");
        }
    }

    if (ctx->eStandard == IMG_STANDARD_MPEG4) {
	if (ctx->ui32FrameCount[0] == 0) {
	    cmdbuf->cmd_idx_saved[PTG_CMDBUF_PIC_HEADER_IDX] = cmdbuf->cmd_idx;
            ptg_cmdbuf_insert_command_package(ctx->obj_context, 0,
                                              MTX_CMDID_DO_HEADER,
                                              &(ps_mem->bufs_seq_header),
                                              0);
	}
#ifdef _TOPAZHP_VIR_ADDR_
	psb_buffer_map(&(ps_mem->bufs_seq_header), &(ps_mem->bufs_seq_header.virtual_addr));
#endif
	ptg__MPEG4_prepare_sequence_header(ps_mem->bufs_seq_header.virtual_addr,
					   IMG_FALSE,//FIXME: Zhaohan bFrame
					   SP,//profile
					   3,//ui8Profile_lvl_indication
					   3,//ui8Fixed_vop_time_increment
					   ctx->obj_context->picture_width,//ui8Fixed_vop_time_increment
					   ctx->obj_context->picture_height,//ui32Picture_Height_Pixels
					   NULL,//VBVPARAMS
					   ctx->ui32VopTimeResolution);
#ifdef _TOPAZHP_VIR_ADDR_
	psb_buffer_unmap(&(ps_mem->bufs_seq_header));
#endif

	cmdbuf->cmd_idx_saved[PTG_CMDBUF_SEQ_HEADER_IDX] = cmdbuf->cmd_idx;
    }

    ptg_cmdbuf_insert_command_package(ctx->obj_context, ctx->ui32StreamID,
        MTX_CMDID_ENCODE_FRAME, 0, 0);
/*
    ptg_cmdbuf_insert_command_package(ctx->obj_context, 0,
        MTX_CMDID_SHUTDOWN, 0, 0);
*/

#ifdef _TOPAZHP_CMDBUF_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s addr = 0x%08x \n", __FUNCTION__, cmdbuf);
    ptg__trace_cmdbuf_words(cmdbuf);
#endif

#ifdef _TOPAZHP_PDUMP_
    ptg__trace_cmdbuf(cmdbuf, ctx->ui32StreamID);
#endif
//    ptg_buffer_unmap(ctx, ctx->ui32StreamID);
    ptg_cmdbuf_mem_unmap(cmdbuf);

    /* save current settings */
    ps_buf->previous_src_surface = ps_buf->src_surface;
    ps_buf->previous_ref_surface = ps_buf->ref_surface;

    /*Frame Skip flag in Coded Buffer of frame N determines if frame N+2
    * should be skipped, which means sending encoding commands of frame N+1 doesn't
    * have to wait until frame N is completed encoded. It reduces the precision of
    * rate control but improves HD encoding performance a lot.*/
    ps_buf->pprevious_coded_buf = ps_buf->previous_coded_buf;
    ps_buf->previous_coded_buf = ps_buf->coded_buf;


    if (ptg_context_flush_cmdbuf(ctx->obj_context)) {
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
    }

    ++(ctx->ui32FrameCount[ctx->ui32StreamID]);
    return vaStatus;

}


