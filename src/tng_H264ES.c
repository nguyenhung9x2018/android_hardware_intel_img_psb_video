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
 *    Elaine Wang <elaine.wang@intel.com>
 *    Zeng Li <zeng.li@intel.com>
 *    Edward Lin <edward.lin@intel.com>
 *
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "hwdefs/coreflags.h"
#include "hwdefs/topaz_vlc_regs.h"
#include "hwdefs/topaz_db_regs.h"

#include "va/va_enc_h264.h"

#include "psb_def.h"
#include "psb_drv_debug.h"
#include "psb_surface.h"
#include "tng_cmdbuf.h"
#include "tng_hostcode.h"
#include "tng_hostheader.h"
#include "tng_picmgmt.h"
#include "tng_slotorder.h"
#include "tng_hostair.h"
#include "tng_H264ES.h"

#define TOPAZ_H264_MAX_BITRATE 135000000

#define INIT_CONTEXT_H264ES     context_ENC_p ctx = (context_ENC_p) obj_context->format_data
#define SURFACE(id)    ((object_surface_p) object_heap_lookup( &ctx->obj_context->driver_data->surface_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &ctx->obj_context->driver_data->buffer_heap, id ))

#ifdef _TOPAZHP_PDUMP_ALL_
#define _PDUMP_H264ES_FUNC_
#endif

static VAStatus tng__H264ES_init_profile(
    object_context_p obj_context,
    object_config_p obj_config)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_ENC_p ctx;
    ctx = (context_ENC_p) obj_context->format_data;
    switch (obj_config->profile) {
        case VAProfileH264Baseline:
        case VAProfileH264ConstrainedBaseline:
            ctx->ui8ProfileIdc = H264ES_PROFILE_BASELINE;
            break;
        case VAProfileH264Main:
            ctx->ui8ProfileIdc = H264ES_PROFILE_MAIN;
            break;
        case VAProfileH264High:
            ctx->ui8ProfileIdc = H264ES_PROFILE_HIGH;
            break;
        case VAProfileH264StereoHigh:
            ctx->ui8ProfileIdc = H264ES_PROFILE_HIGH;
            ctx->bEnableMVC = 1;
            ctx->ui16MVCViewIdx = 0;
            break;
        default:
            ctx->ui8ProfileIdc = H264ES_PROFILE_BASELINE;
            vaStatus = VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
            break;
    }
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s: obj_config->profile = %dctx->eStandard = %d, ctx->bEnableMVC = %d\n",
        __FUNCTION__, obj_config->profile, ctx->eStandard, ctx->bEnableMVC);
#endif
    return vaStatus;
}

static VAStatus tng__H264ES_get_codec_type(
    object_context_p obj_context,
    object_config_p obj_config)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_ENC_p ctx = (context_ENC_p) obj_context->format_data;
    IMG_RC_PARAMS *psRCParams = &(ctx->sRCParams);

    ctx->eCodec = IMG_CODEC_NUM;

    if (ctx->bEnableMVC) {
        switch (psRCParams->eRCMode) {
            case IMG_RCMODE_NONE:
                ctx->eCodec = IMG_CODEC_H264MVC_NO_RC;
                break;
            case IMG_RCMODE_CBR:
                ctx->eCodec = IMG_CODEC_H264MVC_CBR;
                break;
            case IMG_RCMODE_VBR:
                ctx->eCodec = IMG_CODEC_H264MVC_VBR;
                break;
            default:
                drv_debug_msg(VIDEO_DEBUG_ERROR, "RC mode MVC\n");
                break;
        }
    } else {
        switch (psRCParams->eRCMode) {
            case IMG_RCMODE_NONE:
                ctx->eCodec = IMG_CODEC_H264_NO_RC;
                break;
            case IMG_RCMODE_CBR:
                ctx->eCodec = IMG_CODEC_H264_CBR;
                break;
            case IMG_RCMODE_VBR:
                ctx->eCodec = IMG_CODEC_H264_VBR;
                break;
            case IMG_RCMODE_VCM:
                ctx->eCodec = IMG_CODEC_H264_VCM;
                break;
            default:
                drv_debug_msg(VIDEO_DEBUG_ERROR, "RC mode\n");
                break;
        }
    }
    return vaStatus;
}

static VAStatus tng__H264ES_init_format_mode(
    object_context_p obj_context,
    object_config_p obj_config)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_ENC_p ctx = (context_ENC_p) obj_context->format_data;

#ifdef _TOPAZHP_OLD_LIBVA_
    ctx->bIsInterlaced = IMG_FALSE;
    ctx->bIsInterleaved = IMG_FALSE;
    ctx->ui16PictureHeight = ctx->ui16FrameHeight;
    ctx->eCodec = IMG_CODEC_H264_NO_RC;
#else
    unsigned int eFormatMode;
    int i;

    for (i = 0; i < obj_config->attrib_count; i++) {
        if (obj_config->attrib_list[i].type == VAConfigAttribEncInterlaced)
            break;
    }

    if (i >= obj_config->attrib_count)
        eFormatMode = VA_ENC_INTERLACED_NONE;
    else
        eFormatMode = obj_config->attrib_list[i].value;

    if (eFormatMode == VA_ENC_INTERLACED_NONE) {
        ctx->bIsInterlaced = IMG_FALSE;
        ctx->bIsInterleaved = IMG_FALSE;
        ctx->ui16PictureHeight = ctx->ui16FrameHeight;
        ctx->eCodec = IMG_CODEC_H264_NO_RC;
    } else {
        if (eFormatMode == VA_ENC_INTERLACED_FRAME) {
            ctx->bIsInterlaced = IMG_TRUE;
            ctx->bIsInterleaved = IMG_FALSE;
        } else if (eFormatMode == VA_ENC_INTERLACED_FIELD) {
            ctx->bIsInterlaced = IMG_TRUE;
            ctx->bIsInterleaved = IMG_TRUE;
        } else {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "not support this RT Format\n");
            return VA_STATUS_ERROR_UNKNOWN;
        }
        ctx->ui16PictureHeight = ctx->ui16FrameHeight >> 1;
    }
#endif
    return vaStatus;
}

static void tng__H264ES_init_context(object_context_p obj_context,
    object_config_p obj_config)
{
    context_ENC_p ctx = (context_ENC_p) obj_context->format_data;
    //This parameter need not be exposed
    ctx->ui8InterIntraIndex = 3;
    ctx->ui8CodedSkippedIndex = 3;
    ctx->bEnableHostQP = IMG_FALSE;
    ctx->uMaxChunks = 0xA0;
    ctx->uChunksPerMb = 0x40;
    ctx->uPriorityChunks = (0xA0 - 0x60);
    ctx->ui32FCode = 4;
    ctx->iFineYSearchSize = 2;

    //This parameter need not be exposed
    //host to control the encoding process
    ctx->bEnableInpCtrl = IMG_FALSE;
    ctx->bEnableHostBias = IMG_FALSE;
    //By default false Newly Added
    ctx->bEnableCumulativeBiases = IMG_FALSE;

    //Weighted Prediction is not supported in TopazHP Version 3.0
    ctx->bWeightedPrediction = IMG_FALSE;
    ctx->ui8VPWeightedImplicitBiPred = 0;
    ctx->bInsertHRDParams = 0;
    ctx->bArbitrarySO = IMG_FALSE;
    ctx->ui32BasicUnit = 0;
    ctx->bVPAdaptiveRoundingDisable = IMG_FALSE;
}

static VAStatus tng__H264ES_process_misc_framerate_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAEncMiscParameterBuffer *pBuffer = (VAEncMiscParameterBuffer *) obj_buffer->buffer_data;
    VAEncMiscParameterFrameRate *psMiscFrameRateParam;
    IMG_RC_PARAMS *psRCParams = &(ctx->sRCParams);

    ASSERT(obj_buffer->type == VAEncMiscParameterTypeFrameRate);
    ASSERT(obj_buffer->size == sizeof(VAEncMiscParameterFrameRate));

    psMiscFrameRateParam = (VAEncMiscParameterFrameRate *)(pBuffer->data);

    if (psMiscFrameRateParam == NULL)
        return VA_STATUS_ERROR_INVALID_BUFFER;

    if ((psMiscFrameRateParam->framerate != psRCParams->ui32FrameRate) &&
        (psMiscFrameRateParam->framerate > 0)) {
        psRCParams->ui32FrameRate = psMiscFrameRateParam->framerate;
        psRCParams->bBitrateChanged = IMG_TRUE;
    }

    return VA_STATUS_SUCCESS;
}

static VAStatus tng__H264ES_process_misc_ratecontrol_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAEncMiscParameterRateControl *psMiscRcParams;
    IMG_RC_PARAMS *psRCParams = &(ctx->sRCParams);
    VAEncMiscParameterBuffer *pBuffer = (VAEncMiscParameterBuffer *) obj_buffer->buffer_data;
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start \n", __FUNCTION__);
#endif
    ASSERT(obj_buffer->type == VAEncMiscParameterTypeRateControl);
    ASSERT(obj_buffer->size == sizeof(VAEncMiscParameterRateControl));
    psMiscRcParams = (VAEncMiscParameterRateControl *)pBuffer->data;

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s psRCParams->ui32BitsPerSecond = %d, psMiscRcParams->bits_per_second = %d \n",
        __FUNCTION__, psRCParams->ui32BitsPerSecond, psMiscRcParams->bits_per_second);
#endif

    if (psMiscRcParams->bits_per_second > TOPAZ_H264_MAX_BITRATE) {
#ifdef _PDUMP_H264ES_FUNC_
        drv_debug_msg(VIDEO_DEBUG_GENERAL,
            "%s: bits_per_second(%d) exceeds \ the maximum bitrate, set it with %d\n",
            __FUNCTION__, psMiscRcParams->bits_per_second, TOPAZ_H264_MAX_BITRATE);
#endif
        psMiscRcParams->bits_per_second = TOPAZ_H264_MAX_BITRATE;
    }

    if ((psRCParams->ui32BitsPerSecond != psMiscRcParams->bits_per_second) && 
        psMiscRcParams->bits_per_second != 0) {
        psRCParams->ui32BitsPerSecond = psMiscRcParams->bits_per_second;
        psRCParams->bBitrateChanged = IMG_TRUE;
    }

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s: rate control changed from %d to %d\n", __FUNCTION__,
        psRCParams->ui32BitsPerSecond, psMiscRcParams->bits_per_second);
#endif

    if (psMiscRcParams->window_size != 0)
        ctx->uiCbrBufferTenths = psMiscRcParams->window_size / 100;

    if (ctx->uiCbrBufferTenths) {
        psRCParams->ui32BufferSize = (IMG_UINT32)(psRCParams->ui32BitsPerSecond * ctx->uiCbrBufferTenths / 10.0);
    } else {
        if (psRCParams->ui32BitsPerSecond < 256000)
            psRCParams->ui32BufferSize = ((9 * psRCParams->ui32BitsPerSecond) >> 1);
        else
            psRCParams->ui32BufferSize = ((5 * psRCParams->ui32BitsPerSecond) >> 1);
    }

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s ctx->uiCbrBufferTenths = %d, psRCParams->ui32BufferSize = %d\n",
        __FUNCTION__, ctx->uiCbrBufferTenths, psRCParams->ui32BufferSize);
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s psRCParams->ui32BitsPerSecond = %d, psMiscRcParams->bits_per_second = %d\n",
        __FUNCTION__, psRCParams->ui32BitsPerSecond, psMiscRcParams->bits_per_second);
#endif

    //psRCParams->ui32BUSize = psMiscRcParams->basic_unit_size;
    psRCParams->i32InitialDelay = (13 * psRCParams->ui32BufferSize) >> 4;
    psRCParams->i32InitialLevel = (3 * psRCParams->ui32BufferSize) >> 4;

    //free(psMiscRcParams);
    if (psMiscRcParams->initial_qp > 51) {
        drv_debug_msg(VIDEO_DEBUG_ERROR,
            "%s: Initial_qp(%d) are invalid.\nQP shouldn't be larger than 51 for H264\n",
            __FUNCTION__, psMiscRcParams->initial_qp);
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    //free(psMiscRcParams);
    if (psMiscRcParams->min_qp > 51 || psMiscRcParams->min_qp < 0) {
        drv_debug_msg(VIDEO_DEBUG_ERROR,
            "%s: min_qpinitial_qp(%d) are invalid.\nQP shouldn't be larger than 51 for H264\n",
             __FUNCTION__, psMiscRcParams->min_qp );
        psMiscRcParams->min_qp = 10;
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    if ((psRCParams->eRCMode == IMG_RCMODE_NONE) ||
        (psRCParams->ui32InitialQp == 0)) {
        psRCParams->ui32InitialQp = psMiscRcParams->initial_qp;
        psRCParams->iMinQP = psMiscRcParams->min_qp;
    }

    return VA_STATUS_SUCCESS;
}

static VAStatus tng__H264ES_process_misc_hrd_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAEncMiscParameterBuffer *pBuffer = (VAEncMiscParameterBuffer *) obj_buffer->buffer_data;
    VAEncMiscParameterHRD *psMiscHrdParams = NULL;
    ASSERT(obj_buffer->type == VAEncMiscParameterTypeHRD);
    ASSERT(obj_buffer->size == sizeof(VAEncMiscParameterHRD));
    psMiscHrdParams = (VAEncMiscParameterHRD *)pBuffer->data;
 
    return VA_STATUS_SUCCESS;
}

//APP_SetVideoParams
static VAStatus tng__H264ES_process_misc_air_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAEncMiscParameterBuffer *pBuffer = (VAEncMiscParameterBuffer *) obj_buffer->buffer_data;
    VAEncMiscParameterAIR *psMiscAirParams = NULL;
    ADAPTIVE_INTRA_REFRESH_INFO_TYPE *psAIRInfo = &(ctx->sAirInfo);
    IMG_UINT32 ui32MbNum;
    ASSERT(obj_buffer->type == VAEncMiscParameterTypeAIR);
    ASSERT(obj_buffer->size == sizeof(VAEncMiscParameterAIR));
    
    psMiscAirParams = (VAEncMiscParameterAIR*)pBuffer->data;
    ui32MbNum = (ctx->ui16PictureHeight * ctx->ui16Width) >> 8;

    printf("%s: enable_AIR = %d, mb_num = %d, thresh_hold = %d\n", __FUNCTION__,
        ctx->bEnableAIR, psMiscAirParams->air_num_mbs, psMiscAirParams->air_threshold);

    ctx->bEnableAIR = 1;
    ctx->bEnableInpCtrl = 1;
    ctx->bEnableHostBias = 1;
    ctx->ui8EnableSelStatsFlags |= ESF_FIRST_STAGE_STATS;
    ctx->ui8EnableSelStatsFlags |= ESF_MP_BEST_MB_DECISION_STATS;

    if (psMiscAirParams->air_threshold == -1 && psMiscAirParams->air_num_mbs == 0) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, 
            "%s: ERROR: Cannot have both -AIRMBperFrame set to zero"
            "AND and -AIRSADThreshold set to -1 (APP_SetVideoParams)\n",
            __FUNCTION__);
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    if (psMiscAirParams->air_num_mbs > ui32MbNum)
        psMiscAirParams->air_num_mbs = ui32MbNum;

    if (psMiscAirParams->air_threshold > 65535) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, 
            "%s: ERROR: air_threshold = %d should not bigger than 65536\n",
            __FUNCTION__, psMiscAirParams->air_threshold);
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    psAIRInfo->i32NumAIRSPerFrame = psMiscAirParams->air_num_mbs;
    psAIRInfo->i32SAD_Threshold = psMiscAirParams->air_threshold;
    psAIRInfo->i16AIRSkipCnt = 0;   //psMiscAirParams->i16AIRSkipCnt;
    psAIRInfo->ui16AIRScanPos = 0;

 #ifdef _PDUMP_H264ES_FUNC_ 
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s: air slice size changed to num_air_mbs %d "
        "air_threshold %d, air_auto %d\n", __FUNCTION__,
        psMiscAirParams->air_num_mbs, psMiscAirParams->air_threshold,
        psMiscAirParams->air_auto);
#endif
    if (psAIRInfo->pi8AIR_Table != NULL)
        free(psAIRInfo->pi8AIR_Table);
    tng_air_buf_create(ctx);
    
    return VA_STATUS_SUCCESS;
}


static IMG_UINT8 tng__H264ES_calculate_level(context_ENC_p ctx)
{
    IMG_RC_PARAMS *psRCParams = &(ctx->sRCParams);
    IMG_UINT32 ui32MBf=0;
    IMG_UINT32 ui32MBs;
    IMG_UINT32 ui32Level=0;
    IMG_UINT32 ui32TempLevel=0;
    IMG_UINT32 ui32DpbMbs;
    // We need to calculate the level based on a few constraints these are
    // Macroblocks per second
    // picture size
    // decoded picture buffer size, and bitrate
    ui32MBf = (ctx->ui16Width)*(ctx->ui16FrameHeight) >> 8;
    ui32MBs = ui32MBf * psRCParams->ui32FrameRate;


    // could do these in nice tables, but this is clearer
    if      (ui32MBf > 22080) ui32Level = SH_LEVEL_51;
    else if (ui32MBf >  8704) ui32Level = SH_LEVEL_50;
    else if (ui32MBf >  8192) ui32Level = SH_LEVEL_42;
    else if (ui32MBf >  5120) ui32Level = SH_LEVEL_40;
    else if (ui32MBf >  3600) ui32Level = SH_LEVEL_32;
    else if (ui32MBf >  1620) ui32Level = SH_LEVEL_31;
    else if (ui32MBf >   792) ui32Level = SH_LEVEL_22;
    else if (ui32MBf >   396) ui32Level = SH_LEVEL_21;
    else if (ui32MBf >    99) ui32Level = SH_LEVEL_11;
    else ui32Level = SH_LEVEL_10;

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s: ui32MBf = %d, ui32MBs = %d, ui32Level = %d\n",
        __FUNCTION__, ui32MBf, ui32MBs, ui32Level);
#endif

    //ui32DpbMbs = ui32MBf * (psContext->ui32MaxNumRefFrames + 1);
    ui32DpbMbs = ui32MBf * (ctx->ui8MaxNumRefFrames + 1);

    if      (ui32DpbMbs > 110400) ui32TempLevel = SH_LEVEL_51;
    else if (ui32DpbMbs >  34816) ui32TempLevel = SH_LEVEL_50;
    else if (ui32DpbMbs >  32768) ui32TempLevel = SH_LEVEL_42;
    else if (ui32DpbMbs >  20480) ui32TempLevel = SH_LEVEL_40;
    else if (ui32DpbMbs >  18000) ui32TempLevel = SH_LEVEL_32;
    else if (ui32DpbMbs >   8100) ui32TempLevel = SH_LEVEL_31;
    else if (ui32DpbMbs >   4752) ui32TempLevel = SH_LEVEL_22;
    else if (ui32DpbMbs >   2376) ui32TempLevel = SH_LEVEL_21;
    else if (ui32DpbMbs >    900) ui32TempLevel = SH_LEVEL_12;
    else if (ui32DpbMbs >    396) ui32TempLevel = SH_LEVEL_11;
    else ui32TempLevel = SH_LEVEL_10;
    ui32Level = tng__max(ui32Level, ui32TempLevel);

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s: ui32DpbMbs = %d, ui32Level = %d\n",
        __FUNCTION__, ui32DpbMbs, ui32Level);
#endif
    // now restrict based on the number of macroblocks per second
    if      (ui32MBs > 589824) ui32TempLevel = SH_LEVEL_51;
    else if (ui32MBs > 522240) ui32TempLevel = SH_LEVEL_50;
    else if (ui32MBs > 245760) ui32TempLevel = SH_LEVEL_42;
    else if (ui32MBs > 216000) ui32TempLevel = SH_LEVEL_40;
    else if (ui32MBs > 108000) ui32TempLevel = SH_LEVEL_32;
    else if (ui32MBs >  40500) ui32TempLevel = SH_LEVEL_31;
    else if (ui32MBs >  20250) ui32TempLevel = SH_LEVEL_30;
    else if (ui32MBs >  19800) ui32TempLevel = SH_LEVEL_22;
    else if (ui32MBs >  11880) ui32TempLevel = SH_LEVEL_21;
    else if (ui32MBs >   6000) ui32TempLevel = SH_LEVEL_13;
    else if (ui32MBs >   3000) ui32TempLevel = SH_LEVEL_12;
    else if (ui32MBs >   1485) ui32TempLevel = SH_LEVEL_11;
    else ui32TempLevel = SH_LEVEL_10;
    ui32Level = tng__max(ui32Level, ui32TempLevel);

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s: ui32TempLevel = %d, ui32Level = %d\n",
        __FUNCTION__, ui32TempLevel, ui32Level);
#endif

    if (psRCParams->bRCEnable) {
        // now restrict based on the requested bitrate
        if      (psRCParams->ui32FrameRate > 135000000) ui32TempLevel = SH_LEVEL_51;
        else if (psRCParams->ui32FrameRate >  50000000) ui32TempLevel = SH_LEVEL_50;
        else if (psRCParams->ui32FrameRate >  20000000) ui32TempLevel = SH_LEVEL_41;
        else if (psRCParams->ui32FrameRate >  14000000) ui32TempLevel = SH_LEVEL_32;
        else if (psRCParams->ui32FrameRate >  10000000) ui32TempLevel = SH_LEVEL_31;
        else if (psRCParams->ui32FrameRate >   4000000) ui32TempLevel = SH_LEVEL_30;
        else if (psRCParams->ui32FrameRate >   2000000) ui32TempLevel = SH_LEVEL_21;
        else if (psRCParams->ui32FrameRate >    768000) ui32TempLevel = SH_LEVEL_20;
        else if (psRCParams->ui32FrameRate >    384000) ui32TempLevel = SH_LEVEL_13;
        else if (psRCParams->ui32FrameRate >    192000) ui32TempLevel = SH_LEVEL_12;
        else if (psRCParams->ui32FrameRate >    128000) ui32TempLevel = SH_LEVEL_11;
        else if (psRCParams->ui32FrameRate >     64000) ui32TempLevel = SH_LEVEL_1B;
        else ui32TempLevel = SH_LEVEL_10;

        ui32Level = tng__max(ui32Level, ui32TempLevel);
    }
/*
    if (pParams->bLossless) {
        ui32Level = tng__max(ui32Level, 320);
    }
*/
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s: target level is %d, input level is %d\n",
        __FUNCTION__, ui32Level, ctx->ui8LevelIdc);
#endif
    return (IMG_UINT8)ui32Level;
}

static VAStatus tng__H264ES_process_sequence_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAEncSequenceParameterBufferH264 *psSeqParams;
    H264_CROP_PARAMS* psCropParams = &(ctx->sCropParams);
    IMG_RC_PARAMS *psRCParams = &(ctx->sRCParams);
    FRAME_ORDER_INFO *psFrameInfo = &(ctx->sFrameOrderInfo);
    IMG_UINT32 ui32_var_0, ui32_var_1;

    ASSERT(obj_buffer->type == VAEncSequenceParameterBufferType);
    ASSERT(obj_buffer->size == sizeof(VAEncSequenceParameterBufferH264));

    if (obj_buffer->size != sizeof(VAEncSequenceParameterBufferH264)) {
        return VA_STATUS_ERROR_UNKNOWN;
    }
    ctx->obj_context->frame_count = 0;
    psSeqParams = (VAEncSequenceParameterBufferH264 *) obj_buffer->buffer_data;
    obj_buffer->buffer_data = NULL;
    obj_buffer->size = 0;

    ctx->ui32IdrPeriod = psSeqParams->intra_idr_period;
    ctx->ui32IntraCnt = psSeqParams->intra_period;
    ctx->ui8LevelIdc = psSeqParams->level_idc;
    ctx->ui8SlotsInUse = (IMG_UINT8)(psSeqParams->ip_period + 1);
    ctx->ui8MaxNumRefFrames = psSeqParams->max_num_ref_frames;

    //be sure intral period is the cycle of ip_period
    ui32_var_0 = (IMG_UINT32)(psSeqParams->ip_period);
    ui32_var_1 = ctx->ui32IntraCnt % ui32_var_0;
	if (ui32_var_1 != 0)
        ctx->ui32IntraCnt = ctx->ui32IntraCnt + ui32_var_0 - ui32_var_1;

    //bits per second
    if (!psSeqParams->bits_per_second) {
        psSeqParams->bits_per_second = ctx->ui16Width * ctx->ui16PictureHeight * 30 * 12;
#ifdef _PDUMP_H264ES_FUNC_
        drv_debug_msg(VIDEO_DEBUG_GENERAL,
            "%s: bits_per_second is 0, set to %d\n",
            __FUNCTION__, psSeqParams->bits_per_second);
#endif
    }

    if (psSeqParams->bits_per_second > TOPAZ_H264_MAX_BITRATE) {
        psSeqParams->bits_per_second = TOPAZ_H264_MAX_BITRATE;
#ifdef _PDUMP_H264ES_FUNC_
        drv_debug_msg(VIDEO_DEBUG_GENERAL,
            "%s: bits_per_second(%d) exceeds the maximum bitrate, set it with %d\n",
            __FUNCTION__, psSeqParams->bits_per_second,
            TOPAZ_H264_MAX_BITRATE);
#endif
    }

    if (psRCParams->ui32BitsPerSecond == 0)
        psRCParams->ui32BitsPerSecond = psSeqParams->bits_per_second;

    if (psSeqParams->bits_per_second != psRCParams->ui32BitsPerSecond) {
        psRCParams->ui32BitsPerSecond = psSeqParams->bits_per_second;
        psRCParams->bBitrateChanged = IMG_TRUE;
    }

    psRCParams->ui32IntraFreq = psSeqParams->intra_period;
    psRCParams->ui32TransferBitsPerSecond = psRCParams->ui32BitsPerSecond;
    psRCParams->ui16BFrames = (IMG_UINT16)(psSeqParams->ip_period - 1);

    if (psRCParams->ui32FrameRate == 0)
        psRCParams->ui32FrameRate = 30;

    //set the B frames
    if (psRCParams->eRCMode == IMG_RCMODE_VCM)
        psRCParams->ui16BFrames = 0;

    if ((psRCParams->ui16BFrames > 0) && (ctx->ui8ProfileIdc == H264ES_PROFILE_BASELINE)) {
        ctx->ui8ProfileIdc = H264ES_PROFILE_MAIN;
    }

    if (psFrameInfo->slot_consume_dpy_order != NULL)
        free(psFrameInfo->slot_consume_dpy_order);
    if (psFrameInfo->slot_consume_enc_order != NULL)
        free(psFrameInfo->slot_consume_enc_order);

    if (psRCParams->ui16BFrames != 0) {
        memset(psFrameInfo, 0, sizeof(FRAME_ORDER_INFO));
        psFrameInfo->slot_consume_dpy_order = (int *)malloc(ctx->ui8SlotsInUse * sizeof(int));
        psFrameInfo->slot_consume_enc_order = (int *)malloc(ctx->ui8SlotsInUse * sizeof(int));

        if ((psFrameInfo->slot_consume_dpy_order == NULL) || 
            (psFrameInfo->slot_consume_enc_order == NULL)) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: error malloc slot order array\n", __FUNCTION__);
        }
    }

    //set the crop parameters
    psCropParams->bClip = psSeqParams->frame_cropping_flag;
    psCropParams->ui16LeftCropOffset = psSeqParams->frame_crop_left_offset;
    psCropParams->ui16RightCropOffset = psSeqParams->frame_crop_right_offset;
    psCropParams->ui16TopCropOffset = psSeqParams->frame_crop_top_offset;
    psCropParams->ui16BottomCropOffset = psSeqParams->frame_crop_bottom_offset;

#ifdef _PDUMP_H264ES_FUNC_
        drv_debug_msg(VIDEO_DEBUG_GENERAL,
            "%s ctx->ui32IdrPeriod = %d, ctx->ui32IntraCnt = %d\n",
            __FUNCTION__, ctx->ui32IdrPeriod, ctx->ui32IntraCnt);
        drv_debug_msg(VIDEO_DEBUG_GENERAL,
            "%s ctx->ui8MaxNumRefFrames = %d, psRCParams->ui16BFrames = %d\n",
            __FUNCTION__, ctx->ui8MaxNumRefFrames, psRCParams->ui16BFrames);
        drv_debug_msg(VIDEO_DEBUG_GENERAL,
            "%s bClip = %d\n", __FUNCTION__, psCropParams->bClip);
#endif

    //set level idc parameter
    ctx->ui32VertMVLimit = 255 ;//(63.75 in qpel increments)
    ctx->bLimitNumVectors = IMG_FALSE;

    if (ctx->ui8LevelIdc == 111)
        ctx->ui8LevelIdc = SH_LEVEL_1B;

    ctx->ui8LevelIdc = tng__H264ES_calculate_level(ctx);

    /*Setting VertMVLimit and LimitNumVectors only for H264*/
    if (ctx->ui8LevelIdc >= SH_LEVEL_30)
        ctx->bLimitNumVectors = IMG_TRUE;
    else
        ctx->bLimitNumVectors = IMG_FALSE;

    if (ctx->ui8LevelIdc >= SH_LEVEL_31)
        ctx->ui32VertMVLimit = 2047 ;//(511.75 in qpel increments)
    else if (ctx->ui8LevelIdc >= SH_LEVEL_21)
        ctx->ui32VertMVLimit = 1023 ;//(255.75 in qpel increments)
    else if (ctx->ui8LevelIdc >= SH_LEVEL_11)
        ctx->ui32VertMVLimit = 511 ;//(127.75 in qpel increments)

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s ctx->ui8LevelIdc (%d), ctx->bLimitNumVectors (%d), ctx->ui32VertMVLimit (%d)\n",
        __FUNCTION__, ctx->ui8LevelIdc, ctx->bLimitNumVectors, ctx->ui32VertMVLimit);
#endif

//    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s end\n", __FUNCTION__);
    free(psSeqParams);

    return VA_STATUS_SUCCESS;
}

#if 0
static VAStatus tng__H264ES_process_picture_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_ENC_mem *ps_mem = &(ctx->ctx_mem[ctx->ui32StreamID]);
    context_ENC_frame_buf *ps_buf = &(ctx->ctx_frame_buf[ctx->ui32StreamID]);
    IMG_RC_PARAMS * psRCParams = &(ctx->sRCParams);
    VAEncPictureParameterBufferH264 *psPicParams;
    IMG_BOOL bDepViewPPS = IMG_FALSE;

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: start\n",__FUNCTION__);
    ASSERT(obj_buffer->type == VAEncPictureParameterBufferType);
    if (obj_buffer->size != sizeof(VAEncPictureParameterBufferH264)) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s L%d Invalid coded buffer handle\n", __FUNCTION__, __LINE__);
        return VA_STATUS_ERROR_UNKNOWN;
    }

    /* Transfer ownership of VAEncPictureParameterBufferH264 data */
    psPicParams = (VAEncPictureParameterBufferH264 *) obj_buffer->buffer_data;

    obj_buffer->buffer_data = NULL;
    obj_buffer->size = 0;

    ASSERT(ctx->ui16Width == psPicParams->picture_width);
    ASSERT(ctx->ui16PictureHeight == psPicParams->picture_height);

#ifdef _TOPAZHP_OLD_LIBVA_
    ps_buf->ref_surface = SURFACE(psPicParams->ReferenceFrames[0].picture_id);
    ps_buf->coded_buf = BUFFER(psPicParams->coded_buf);
#else
    {
        IMG_INT32 i;
        ps_buf->rec_surface = SURFACE(psPicParams->CurrPic.picture_id);
        for (i = 0; i < 16; i++)
            ps_buf->ref_surface[i] = SURFACE(psPicParams->ReferenceFrames[i].picture_id);
        ps_buf->coded_buf = BUFFER(psPicParams->coded_buf);
    }
#endif

    if (NULL == ps_buf->coded_buf) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s L%d Invalid coded buffer handle\n", __FUNCTION__, __LINE__);
        free(psPicParams);
        return VA_STATUS_ERROR_INVALID_BUFFER;
    }

    if ((ctx->bEnableMvc) && (ctx->ui16MVCViewIdx != 0) &&
        (ctx->ui16MVCViewIdx != (IMG_UINT16)(NON_MVC_VIEW))) {
        bDepViewPPS = IMG_TRUE;
    }

    /************* init ****************
    ctx->bCabacEnabled = psPicParams->pic_fields.bits.entropy_coding_mode_flag;
    ctx->bH2648x8Transform = psPicParams->pic_fields.bits.transform_8x8_mode_flag;
    ctx->bH264IntraConstrained = psPicParams->pic_fields.bits.constrained_intra_pred_flag;
    ctx->bWeightedPrediction = psPicParams->pic_fields.bits.weighted_pred_flag;
    ctx->ui8VPWeightedImplicitBiPred = psPicParams->pic_fields.bits.weighted_bipred_idc;
    ctx->bCustomScaling = psPicParams->pic_fields.bits.pic_scaling_matrix_present_flag;
    ************* set rc params *************/
//    ctx->sRCParams.ui32InitialQp = psPicParams->pic_init_qp;
//    ctx->sRCParams.i8QCPOffset = psPicParams->chroma_qp_index_offset;
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s psRCParams->ui32InitialQp = %d, psRCParams->iMinQP = %d\n", __FUNCTION__, psPicParams->pic_init_qp, psPicParams->chroma_qp_index_offset);
    tng__H264ES_prepare_picture_header(
        ps_mem->bufs_pic_template.virtual_addr,
        0, //IMG_BOOL    bCabacEnabled,
        ctx->bH2648x8Transform, //IMG_BOOL    b_8x8transform,
        0, //IMG_BOOL    bIntraConstrained,
        0, //IMG_INT8    i8CQPOffset,
        0, //IMG_BOOL    bWeightedPrediction,
        0, //IMG_UINT8   ui8WeightedBiPred,
        0, //IMG_BOOL    bMvcPPS,
        0, //IMG_BOOL    bScalingMatrix,
        0  //IMG_BOOL    bScalingLists
    );
    free(psPicParams);
/*
    if (psRCParams->ui16BFrames == 0) {
        tng_send_codedbuf(ctx, (ctx->obj_context->frame_count&1));
        tng_send_source_frame(ctx, (ctx->obj_context->frame_count&1), ctx->obj_context->frame_count);
    } else
        tng__H264ES_provide_buffer_for_BFrames(ctx);
*/
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: end\n",__FUNCTION__);

    return vaStatus;
}
#endif

static VAStatus tng__H264ES_process_picture_param_base(context_ENC_p ctx, unsigned char *buf)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_ENC_frame_buf *ps_buf = &(ctx->ctx_frame_buf);
    VAEncPictureParameterBufferH264 *psPicParams;
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: start\n",__FUNCTION__);
#endif
    psPicParams = (VAEncPictureParameterBufferH264 *) buf;

    ASSERT(ctx->ui16Width == psPicParams->picture_width);
    ASSERT(ctx->ui16PictureHeight == psPicParams->picture_height);

#ifdef _TOPAZHP_OLD_LIBVA_
    ps_buf->rec_surface  = SURFACE(psPicParams->CurrPic.picture_id);
    ps_buf->ref_surface  = SURFACE(psPicParams->ReferenceFrames[0].picture_id);
    ps_buf->ref_surface1 = SURFACE(psPicParams->ReferenceFrames[1].picture_id);
    ps_buf->coded_buf    = BUFFER(psPicParams->coded_buf);
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: psPicParams->coded_buf = 0x%08x, ps_buf->coded_buf = 0x%08x\n",
        __FUNCTION__, psPicParams->coded_buf, ps_buf->coded_buf);
#endif
#else
    {
        IMG_INT32 i;
        ps_buf->rec_surface = SURFACE(psPicParams->CurrPic.picture_id);
        for (i = 0; i < 16; i++)
            ps_buf->ref_surface[i] = SURFACE(psPicParams->ReferenceFrames[i].picture_id);
        ps_buf->coded_buf = BUFFER(psPicParams->coded_buf);
    }
#endif

    if (NULL == ps_buf->coded_buf) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s L%d Invalid coded buffer handle\n", __FUNCTION__, __LINE__);
        free(psPicParams);
        return VA_STATUS_ERROR_INVALID_BUFFER;
    }

    ctx->bH2648x8Transform = psPicParams->pic_fields.bits.transform_8x8_mode_flag;
    if ((ctx->bH2648x8Transform == 1) && (ctx->ui8ProfileIdc != H264ES_PROFILE_HIGH)) {
        drv_debug_msg(VIDEO_DEBUG_ERROR,
        "%s L%d only high profile could set bH2648x8Transform TRUE\n",
        __FUNCTION__, __LINE__);
        ctx->bH2648x8Transform = 0;
    }

    ctx->bH264IntraConstrained = psPicParams->pic_fields.bits.constrained_intra_pred_flag;
    if (ctx->bEnableMVC == 1) {
        drv_debug_msg(VIDEO_DEBUG_ERROR,
            "%s L%d MVC could not set bH264IntraConstrained TRUE\n",
            __FUNCTION__, __LINE__);
        ctx->bH264IntraConstrained = 0;
    }

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s: ctx->bH2648x8Transform = 0x%08x, ctx->bH264IntraConstrained = 0x%08x\n",
        __FUNCTION__, ctx->bH2648x8Transform, ctx->bH264IntraConstrained);
#endif
    ctx->bCabacEnabled = psPicParams->pic_fields.bits.entropy_coding_mode_flag;
    ctx->bWeightedPrediction = psPicParams->pic_fields.bits.weighted_pred_flag;
    ctx->ui8VPWeightedImplicitBiPred = psPicParams->pic_fields.bits.weighted_bipred_idc;

    /************* init ****************
    ctx->bCustomScaling = psPicParams->pic_fields.bits.pic_scaling_matrix_present_flag;
    ************* set rc params *************/
    if (ctx->sRCParams.ui32InitialQp == 0)
        ctx->sRCParams.ui32InitialQp = psPicParams->pic_init_qp;

    ctx->ui32LastPicture = psPicParams->last_picture;

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: end\n",__FUNCTION__);
#endif
    return vaStatus;
}

static VAStatus tng__H264ES_process_picture_param_mvc(context_ENC_p ctx, unsigned char *buf)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    VAEncPictureParameterBufferH264MVC *psPicMvcParams;

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: start\n",__FUNCTION__);
#endif
    psPicMvcParams = (VAEncPictureParameterBufferH264MVC *) buf;
    ctx->ui16MVCViewIdx = ctx->ui32StreamID = psPicMvcParams->view_id;
    vaStatus = tng__H264ES_process_picture_param_base(ctx, (unsigned char*)&(psPicMvcParams->base_picture_param));

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: end\n",__FUNCTION__);
#endif
    return vaStatus;
}

static VAStatus tng__H264ES_process_picture_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;

#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s: start\n",__FUNCTION__);
    ASSERT(obj_buffer->type == VAEncPictureParameterBufferType);

    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s: ctx->bEnableMVC = %d\n", __FUNCTION__, ctx->bEnableMVC);
#endif

    if (ctx->bEnableMVC) {
        if (obj_buffer->size != sizeof(VAEncPictureParameterBufferH264MVC)) {
            drv_debug_msg(VIDEO_DEBUG_ERROR,
                "%s L%d Invalid picture parameter H264 mvc buffer handle\n",
                __FUNCTION__, __LINE__);
            return VA_STATUS_ERROR_UNKNOWN;
        }
        vaStatus = tng__H264ES_process_picture_param_mvc(ctx, obj_buffer->buffer_data);
    } else {
        if (obj_buffer->size != sizeof(VAEncPictureParameterBufferH264)) {
            drv_debug_msg(VIDEO_DEBUG_ERROR,
                "%s L%d Invalid picture parameter H264 buffer handle\n",
                __FUNCTION__, __LINE__);
            return VA_STATUS_ERROR_UNKNOWN;
        }
        vaStatus = tng__H264ES_process_picture_param_base(ctx, obj_buffer->buffer_data);
    }
    free(obj_buffer->buffer_data);
    obj_buffer->buffer_data = NULL;
    obj_buffer->size = 0;
    return vaStatus;
}

static VAStatus tng__H264ES_process_slice_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
#ifdef _TOPAZHP_SLICE_PARAM_
    VAEncSliceParameterBufferH264 *psSliceParams;
#else
    VAEncSliceParameterBuffer *psSliceParams;
#endif
    
    ASSERT(obj_buffer->type == VAEncSliceParameterBufferType);
    /* Prepare InParams for macros of current slice, insert slice header, insert do slice command */
    
    /* Transfer ownership of VAEncPictureParameterBufferH264 data */
#ifdef _TOPAZHP_SLICE_PARAM_
    psSliceParams = (VAEncSliceParameterBufferH264*) obj_buffer->buffer_data;
#else
    psSliceParams = (VAEncSliceParameterBuffer*) obj_buffer->buffer_data;
#endif
    obj_buffer->size = 0;
    obj_buffer->buffer_data = NULL;

    //deblocking behaviour
    ctx->bArbitrarySO = IMG_FALSE;
#ifdef _TOPAZHP_SLICE_PARAM_
    ctx->ui8DeblockIDC = psSliceParams->disable_deblocking_filter_idc;
#else
    ctx->ui8DeblockIDC = psSliceParams->slice_flags.bits.disable_deblocking_filter_idc;
#endif
    if ((ctx->ui32StreamID == 0) && (ctx->ui32FrameCount[0] == 0))
        ++ctx->ui8SlicesPerPicture;

    free(psSliceParams);
    return vaStatus;
}

static VAStatus tng__H264ES_process_misc_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    VAEncMiscParameterBuffer *pBuffer;

    ASSERT(obj_buffer->type == VAEncMiscParameterBufferType);
    ASSERT(ctx != NULL);
    /* Transfer ownership of VAEncMiscParameterBuffer data */
    pBuffer = (VAEncMiscParameterBuffer *) obj_buffer->buffer_data;
    obj_buffer->size = 0;
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s: start pBuffer->type = %d\n",
        __FUNCTION__, pBuffer->type);
#endif

    switch (pBuffer->type) {
        case VAEncMiscParameterTypeFrameRate:
            vaStatus = tng__H264ES_process_misc_framerate_param(ctx, obj_buffer);
            break;
        case VAEncMiscParameterTypeRateControl:
            vaStatus = tng__H264ES_process_misc_ratecontrol_param(ctx, obj_buffer);
            break;
        case VAEncMiscParameterTypeHRD:
            vaStatus = tng__H264ES_process_misc_hrd_param(ctx, obj_buffer);
            break;
        case VAEncMiscParameterTypeAIR:
            vaStatus = tng__H264ES_process_misc_air_param(ctx, obj_buffer);
            break;
        default:
            break;
    }
    free(obj_buffer->buffer_data);
    obj_buffer->buffer_data = NULL;
    return vaStatus;

#if 0
    /* Prepare InParams for macros of current slice, insert slice header, insert do slice command */
    VAEncMiscParameterBuffer *pBuffer;
    VAEncMiscParameterRateControl *rate_control_param;
    VAEncMiscParameterAIR *air_param;
    VAEncMiscParameterMaxSliceSize *max_slice_size_param;
    VAEncMiscParameterFrameRate *frame_rate_param;


    ASSERT(obj_buffer->type == VAEncMiscParameterBufferType);

    /* Transfer ownership of VAEncMiscParameterBuffer data */
    pBuffer = (VAEncMiscParameterBuffer *) obj_buffer->buffer_data;
    obj_buffer->size = 0;

    switch (pBuffer->type) {
    case VAEncMiscParameterTypeFrameRate:
        frame_rate_param = (VAEncMiscParameterFrameRate *)pBuffer->data;

        if (frame_rate_param->framerate > 65535) {
            vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
            break;
        }

        if (ctx->sRCParams.FrameRate == frame_rate_param->framerate)
            break;

        drv_debug_msg(VIDEO_DEBUG_GENERAL, "frame rate changed from %d to %d\n",
                                 ctx->sRCParams.FrameRate,
                                 frame_rate_param->framerate);
        ctx->sRCParams.FrameRate = frame_rate_param->framerate;
        ctx->sRCParams.bBitrateChanged = IMG_TRUE;
        break;

    case VAEncMiscParameterTypeRateControl:
        rate_control_param = (VAEncMiscParameterRateControl *)pBuffer->data;

        if (rate_control_param->initial_qp > 51 ||
            rate_control_param->min_qp > 51) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "Initial_qp(%d) and min_qpinitial_qp(%d) "
                               "are invalid.\nQP shouldn't be larger than 51 for H264\n",
                               rate_control_param->initial_qp, rate_control_param->min_qp);
            vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
            break;
        }

        drv_debug_msg(VIDEO_DEBUG_GENERAL, "rate control changed from %d to %d\n",
                                 ctx->sRCParams.ui32BitsPerSecond,
                                 rate_control_param->bits_per_second);

        if ((rate_control_param->bits_per_second == ctx->sRCParams.ui32BitsPerSecond) &&
            (ctx->sRCParams.ui32BufferSize == ctx->sRCParams.ui32BitsPerSecond / 1000 * rate_control_param->window_size) &&
            (ctx->sRCParams.iMinQP == rate_control_param->min_qp) &&
            (ctx->sRCParams.ui32InitialQp == rate_control_param->initial_qp))
            break;
        else
            ctx->sRCParams.bBitrateChanged = IMG_TRUE;

        if (rate_control_param->bits_per_second > TOPAZ_H264_MAX_BITRATE) {
            ctx->sRCParams.ui32BitsPerSecond = TOPAZ_H264_MAX_BITRATE;
            drv_debug_msg(VIDEO_DEBUG_GENERAL, " bits_per_second(%d) exceeds \
			the maximum bitrate, set it with %d\n",
                                     rate_control_param->bits_per_second,
                                     TOPAZ_H264_MAX_BITRATE);
        } else
            ctx->sRCParams.ui32BitsPerSecond = rate_control_param->bits_per_second;

        if (rate_control_param->window_size != 0)
            ctx->sRCParams.ui32BufferSize = ctx->sRCParams.ui32BitsPerSecond * rate_control_param->window_size / 1000;
        if (rate_control_param->initial_qp != 0)
            ctx->sRCParams.ui32InitialQp = rate_control_param->initial_qp;
        if (rate_control_param->min_qp != 0)
            ctx->sRCParams.iMinQP = rate_control_param->min_qp;
        break;

    case VAEncMiscParameterTypeMaxSliceSize:
        max_slice_size_param = (VAEncMiscParameterMaxSliceSize *)pBuffer->data;

        if (ctx->max_slice_size == max_slice_size_param->max_slice_size)
            break;

        drv_debug_msg(VIDEO_DEBUG_GENERAL, "max slice size changed to %d\n",
                                 max_slice_size_param->max_slice_size);

        ctx->max_slice_size = max_slice_size_param->max_slice_size;

        break;

    case VAEncMiscParameterTypeAIR:
        air_param = (VAEncMiscParameterAIR *)pBuffer->data;

        if (air_param->air_num_mbs > 65535 ||
            air_param->air_threshold > 65535) {
            vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
            break;
        }

        drv_debug_msg(VIDEO_DEBUG_GENERAL, "air slice size changed to num_air_mbs %d "
                                 "air_threshold %d, air_auto %d\n",
                                 air_param->air_num_mbs, air_param->air_threshold,
                                 air_param->air_auto);

        if (((ctx->ui16PictureHeight * ctx->ui16Width) >> 8) < air_param->air_num_mbs)
            air_param->air_num_mbs = ((ctx->ui16PictureHeight * ctx->ui16Width) >> 8);
        if (air_param->air_threshold == 0)
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: air threshold is set to zero\n",
                                     __func__);
        ctx->num_air_mbs = air_param->air_num_mbs;
        ctx->air_threshold = air_param->air_threshold;
        //ctx->autotune_air_flag = air_param->air_auto;

        break;

    default:
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        DEBUG_FAILURE;
        break;
    }

    free(obj_buffer->buffer_data);
    obj_buffer->buffer_data = NULL;
    return vaStatus;
#endif

}

static void tng_H264ES_QueryConfigAttributes(
    VAProfile profile,
    VAEntrypoint entrypoint,
    VAConfigAttrib *attrib_list,
    int num_attribs)
{
    int i;
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s\n", __FUNCTION__);
#endif

    /* RateControl attributes */
    for (i = 0; i < num_attribs; i++) {
        switch (attrib_list[i].type) {
        case VAConfigAttribRTFormat:
            break;

        case VAConfigAttribRateControl:
            attrib_list[i].value = VA_RC_NONE | VA_RC_CBR | VA_RC_VBR | VA_RC_VCM;
            break;

        default:
            attrib_list[i].value = VA_ATTRIB_NOT_SUPPORTED;
            break;
        }
    }
}

static VAStatus tng_H264ES_ValidateConfig(
    object_config_p obj_config)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s\n", __FUNCTION__);
#endif
    return vaStatus;
}

static VAStatus tng_H264ES_setup_profile_features(context_ENC_p ctx)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    IMG_ENCODE_FEATURES * pEncFeatures = &ctx->sEncFeatures;
    pEncFeatures->bEnable8x16MVDetect = IMG_TRUE;
    pEncFeatures->bEnable16x8MVDetect = IMG_TRUE;

    return vaStatus;
}


static VAStatus tng_H264ES_CreateContext(
    object_context_p obj_context,
    object_config_p obj_config)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_ENC_p ctx;

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s\n", __FUNCTION__);

    vaStatus = tng_CreateContext(obj_context, obj_config, 0);

    if (VA_STATUS_SUCCESS != vaStatus)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    ctx = (context_ENC_p) obj_context->format_data;
    ctx->eStandard = IMG_STANDARD_H264;

    tng__H264ES_init_context(obj_context, obj_config);

    vaStatus = tng__H264ES_init_profile(obj_context, obj_config);
    if (vaStatus != VA_STATUS_SUCCESS) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: tng__H264ES_init_profile\n", __FUNCTION__);
    }

    vaStatus = tng__H264ES_init_format_mode(obj_context, obj_config);
    if (vaStatus != VA_STATUS_SUCCESS) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: tng__H264ES_init_format_mode\n", __FUNCTION__);
    }

    vaStatus = tng__H264ES_get_codec_type(obj_context, obj_config);
    if (vaStatus != VA_STATUS_SUCCESS) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: tng__H264ES_init_rc_mode\n", __FUNCTION__);
    }

    vaStatus = tng_H264ES_setup_profile_features(ctx);
    if (vaStatus != VA_STATUS_SUCCESS) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: tng__profile_features\n", __FUNCTION__);
    }

    vaStatus = tng__patch_hw_profile(ctx);
    if (vaStatus != VA_STATUS_SUCCESS) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: tng__patch_hw_profile\n", __FUNCTION__);
    }

    return vaStatus;
}

static void tng_H264ES_DestroyContext(
    object_context_p obj_context)
{
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s\n", __FUNCTION__);
#endif
    tng_DestroyContext(obj_context, 0);
}

static VAStatus tng_H264ES_BeginPicture(
    object_context_p obj_context)
{
    INIT_CONTEXT_H264ES;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s start\n", __FUNCTION__);
#endif
    vaStatus = tng_BeginPicture(ctx);
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s end\n", __FUNCTION__);
#endif
    return vaStatus;
}

static VAStatus tng_H264ES_RenderPicture(
    object_context_p obj_context,
    object_buffer_p *buffers,
    int num_buffers)
{
    INIT_CONTEXT_H264ES;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int i;
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: start\n", __FUNCTION__);
#endif
    for (i = 0; i < num_buffers; i++) {
        object_buffer_p obj_buffer = buffers[i];
#ifdef _PDUMP_H264ES_FUNC_
        drv_debug_msg(VIDEO_DEBUG_GENERAL,
            "%s: type = %d, num = %d\n",
            __FUNCTION__, obj_buffer->type, num_buffers);
#endif
        switch (obj_buffer->type) {
            case VAEncSequenceParameterBufferType:
#ifdef _PDUMP_H264ES_FUNC_
                drv_debug_msg(VIDEO_DEBUG_GENERAL,
                    "tng_H264_RenderPicture got VAEncSequenceParameterBufferType\n");
#endif
                vaStatus = tng__H264ES_process_sequence_param(ctx, obj_buffer);
                DEBUG_FAILURE;
                break;
            case VAEncPictureParameterBufferType:
#ifdef _PDUMP_H264ES_FUNC_
                drv_debug_msg(VIDEO_DEBUG_GENERAL,
                    "tng_H264_RenderPicture got VAEncPictureParameterBuffer\n");
#endif
                vaStatus = tng__H264ES_process_picture_param(ctx, obj_buffer);
                DEBUG_FAILURE;
                break;

            case VAEncSliceParameterBufferType:
#ifdef _PDUMP_H264ES_FUNC_
                drv_debug_msg(VIDEO_DEBUG_GENERAL,
                    "tng_H264_RenderPicture got VAEncSliceParameterBufferType\n");
#endif
                vaStatus = tng__H264ES_process_slice_param(ctx, obj_buffer);
                DEBUG_FAILURE;
                break;

            case VAEncMiscParameterBufferType:
#ifdef _PDUMP_H264ES_FUNC_
                drv_debug_msg(VIDEO_DEBUG_GENERAL,
                    "tng_H264_RenderPicture got VAEncMiscParameterBufferType\n");
#endif
                vaStatus = tng__H264ES_process_misc_param(ctx, obj_buffer);
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
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: end\n", __FUNCTION__);
#endif
    return vaStatus;
}

static VAStatus tng_H264ES_EndPicture(
    object_context_p obj_context)
{
    INIT_CONTEXT_H264ES;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s start\n", __FUNCTION__);
#endif
    vaStatus = tng_EndPicture(ctx);
#ifdef _PDUMP_H264ES_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL,
        "%s end\n", __FUNCTION__);
#endif
    return vaStatus;
}

struct format_vtable_s tng_H264ES_vtable = {
queryConfigAttributes:
    tng_H264ES_QueryConfigAttributes,
validateConfig:
    tng_H264ES_ValidateConfig,
createContext:
    tng_H264ES_CreateContext,
destroyContext:
    tng_H264ES_DestroyContext,
beginPicture:
    tng_H264ES_BeginPicture,
renderPicture:
    tng_H264ES_RenderPicture,
endPicture:
    tng_H264ES_EndPicture
};

/*EOF*/
