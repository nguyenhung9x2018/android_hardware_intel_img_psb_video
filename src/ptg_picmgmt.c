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

/*
 * Authors:
 *    Edward Lin <edward.lin@intel.com>
 *
 */


#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include "hwdefs/topazhp_core_regs.h"
#include "ptg_picmgmt.h"
#include "psb_drv_debug.h"

/************************* MTX_CMDID_PICMGMT *************************/

static VAStatus ptg__update_bitrate(
    context_ENC_p ctx,
    IMG_UINT8     ui32SlotIndex,
    IMG_UINT32    ui32NewBitrate,
    IMG_UINT8     ui8NewVCMIFrameQP
)
{
    object_context_p obj_context_p = NULL;
    ptg_cmdbuf_p   cmdbuf = NULL;
    IMG_PICMGMT_PARAMS  * psPicMgmtParams = NULL;
    IMG_UINT32 uiSlotLen = 0;

    if (!ctx)
        return VA_STATUS_ERROR_UNKNOWN;

    obj_context_p = ctx->obj_context;
    cmdbuf = obj_context_p->ptg_cmdbuf;
    uiSlotLen = ui32SlotIndex * cmdbuf->mem_size;
    psPicMgmtParams = (IMG_PICMGMT_PARAMS *)(cmdbuf->picmgmt_mem_p + uiSlotLen);

    psPicMgmtParams->eSubtype = IMG_PICMGMT_RC_UPDATE;
    psPicMgmtParams->sRCUpdateData.ui32BitsPerFrame = ui32NewBitrate;
    psPicMgmtParams->sRCUpdateData.ui8VCMIFrameQP = ui8NewVCMIFrameQP;

    /* Send PicMgmt Command */
    ptg_cmdbuf_insert_command_package(obj_context_p, ctx->ui32StreamID,
                                      MTX_CMDID_PICMGMT | MTX_CMDID_PRIORITY,
                                      &cmdbuf->picmgmt_mem, uiSlotLen);

    return VA_STATUS_SUCCESS;
}


/**************************************************************************************************
* Function:     IMG_V_SkipFrame
* Description:  Skip the next frame
*
***************************************************************************************************/
IMG_UINT32 ptg_skip_frame(
    context_ENC_p ctx,
    IMG_UINT8     ui32SlotIndex,
    IMG_BOOL      bProcess)
{
    object_context_p obj_context_p = NULL;
    ptg_cmdbuf_p  cmdbuf = NULL;
    IMG_PICMGMT_PARAMS  * psPicMgmtParams = NULL;
    IMG_UINT32 uiSlotLen = 0;

    if (!ctx)
        return VA_STATUS_ERROR_UNKNOWN;

    obj_context_p = ctx->obj_context;
    cmdbuf = obj_context_p->ptg_cmdbuf;
    uiSlotLen = ui32SlotIndex * cmdbuf->mem_size;
    psPicMgmtParams = (IMG_PICMGMT_PARAMS *)(cmdbuf->picmgmt_mem_p + uiSlotLen);

    /* Prepare SkipFrame data */
    psPicMgmtParams->eSubtype = IMG_PICMGMT_SKIP_FRAME;
    psPicMgmtParams->sSkipParams.b8Process = bProcess;

    /* Send PicMgmt Command */
    ptg_cmdbuf_insert_command_package(obj_context_p, ctx->ui32StreamID,
                                      MTX_CMDID_PICMGMT | MTX_CMDID_PRIORITY,
                                      &cmdbuf->picmgmt_mem, uiSlotLen);

    return 0;
}


/**************************************************************************************************
* Function:     IMG_V_SetNextRefType
* Description:  Set the type of the next reference frame
*
***************************************************************************************************/
IMG_UINT32 ptg_set_next_ref_type(
    context_ENC_p  ctx,
    IMG_UINT8      ui32SlotIndex,
    IMG_FRAME_TYPE eFrameType)
{
    object_context_p obj_context_p = NULL;
    ptg_cmdbuf_p  cmdbuf = NULL;
    IMG_PICMGMT_PARAMS  * psPicMgmtParams = NULL;
    IMG_UINT32 uiSlotLen = 0;

    if (!ctx)
        return VA_STATUS_ERROR_UNKNOWN;

    obj_context_p = ctx->obj_context;
    cmdbuf = obj_context_p->ptg_cmdbuf;
    uiSlotLen = ui32SlotIndex * cmdbuf->mem_size;
    psPicMgmtParams = (IMG_PICMGMT_PARAMS *)(cmdbuf->picmgmt_mem_p + uiSlotLen);

    /* Prepare SkipFrame data */
    psPicMgmtParams->eSubtype = IMG_PICMGMT_REF_TYPE;
    psPicMgmtParams->sRefType.eFrameType = eFrameType;

    /* Send PicMgmt Command */
    ptg_cmdbuf_insert_command_package(obj_context_p, ctx->ui32StreamID,
                                      MTX_CMDID_PICMGMT | MTX_CMDID_PRIORITY,
                                      &cmdbuf->picmgmt_mem, uiSlotLen);

    return 0;
}


/**************************************************************************************************
* Function:     IMG_V_EndOfStream
* Description:  End Of Video stream
*
***************************************************************************************************/
IMG_UINT32 ptg_end_of_stream(
    context_ENC_p ctx,
    IMG_UINT8     ui32SlotIndex,
    IMG_UINT32    ui32FrameCount)
{
    object_context_p obj_context_p = NULL;
    ptg_cmdbuf_p  cmdbuf = NULL;
    IMG_PICMGMT_PARAMS  * psPicMgmtParams = NULL;
    IMG_UINT32 uiSlotLen = 0;

    if (!ctx)
        return VA_STATUS_ERROR_UNKNOWN;

    obj_context_p = ctx->obj_context;
    cmdbuf = obj_context_p->ptg_cmdbuf;
    uiSlotLen = ui32SlotIndex * cmdbuf->mem_size;
    psPicMgmtParams = (IMG_PICMGMT_PARAMS *)(cmdbuf->picmgmt_mem_p + uiSlotLen);

    /* Prepare SkipFrame data */
    psPicMgmtParams->eSubtype = IMG_PICMGMT_EOS;
    psPicMgmtParams->sEosParams.ui32FrameCount = ui32FrameCount;

    /* Send PicMgmt Command */
    ptg_cmdbuf_insert_command_package(obj_context_p, ctx->ui32StreamID,
                                      MTX_CMDID_PICMGMT | MTX_CMDID_PRIORITY,
                                      &cmdbuf->picmgmt_mem, uiSlotLen);

    return 0;
}

/*!
******************************************************************************
*
* Picture management functions
*
******************************************************************************/
void ptg__picmgmt_long_term_refs(context_ENC_p ctx, IMG_UINT32 ui32FrameNum)
{
#if _PTG_ENABLE_PITMGMT_
    IMG_BOOL                bIsLongTermRef;
    IMG_BOOL                bUsesLongTermRef0;
    IMG_BOOL                bUsesLongTermRef1;
    IMG_UINT32              ui32FrameCnt;

    // Determine frame position in source stream
    // This assumes there are no IDR frames after the first one
    if (ui32FrameNum == 0) {
        // Initial IDR frame
        ui32FrameCnt = 0;
    } else if (((ui32FrameNum - 1) % (ctx->sRCParams.ui16BFrames + 1)) == 0) {
        // I or P frame
        ui32FrameCnt = ui32FrameNum + ctx->sRCParams.ui16BFrames;
        if (ui32FrameCnt >= ctx->ui32Framecount) ui32FrameCnt = ctx->ui32Framecount - 1;
    } else {
        // B frame
        // This will be incorrect for hierarchical B-pictures
        ui32FrameCnt = ui32FrameNum - 1;
    }

    // Decide if the current frame should be used as a long-term reference
    bIsLongTermRef = ctx->ui32LongTermRefFreq ?
                     (ui32FrameCnt % ctx->ui32LongTermRefFreq == 0) :
                     IMG_FALSE;

    // Decide if the current frame should refer to a long-term reference
    bUsesLongTermRef0 = ctx->ui32LongTermRefUsageFreq ?
                        (ui32FrameCnt % ctx->ui32LongTermRefUsageFreq == ctx->ui32LongTermRefUsageOffset) :
                        IMG_FALSE;
    bUsesLongTermRef1 = IMG_FALSE;

    if (bIsLongTermRef || bUsesLongTermRef0 || bUsesLongTermRef1) {
        // Reconstructed/reference frame to be written to host buffer
        // Send the buffer to be used as reference
        ptg__send_ref_frames(ctx, 0, bIsLongTermRef);
        if (bIsLongTermRef) ctx->byCurBufPointer = (ctx->byCurBufPointer + 1) % 3;
    }
#endif
}

static VAStatus ptg__H264ES_CalcCustomQuantSp(IMG_UINT8 list, IMG_UINT8 param, IMG_UINT8 customQuantQ)
{
    // Derived from sim/topaz/testbench/tests/mved1_tests.c
    IMG_UINT32 mfflat[2][16] = {
        {
            13107, 8066,   13107,  8066,
            8066,   5243,   8066,   5243,
            13107,  8066,   13107,  8066,
            8066,   5243,   8066,   5243
        }, // 4x4
        {
            13107, 12222,  16777,  12222,
            12222,  11428,  15481,  11428,
            16777,  15481,  20972,  15481,
            12222,  11428,  15481,  11428
        } // 8x8
    };
    IMG_UINT8 uVi[2][16] = {
        {
            20, 26,  20,  26,
            26,  32,  26,  32,
            20,  26,  20,  26,
            26,  32,  26,  32
        }, // 4x4
        {
            20, 19,  25,  19,
            19,  18,  24,  18,
            25,  24,  32,  24,
            19,  18,  24,  18
        } // 8x8
    };

    int mfnew;
    double fSp;
    int uSp;

    if (customQuantQ == 0) customQuantQ = 1;
    mfnew = (mfflat[list][param] * 16) / customQuantQ;
    fSp = ((double)(mfnew * uVi[list][param])) / (double)(1 << 22);
    fSp = (fSp * 100000000.0f) / 100000000.0f;
    uSp = (IMG_UINT16)(fSp * 65536);

    return uSp & 0x03FFF;
}


static VAStatus ptg__set_custom_scaling_values(
    context_ENC_p ctx,
    IMG_UINT8* aui8Sl4x4IntraY,
    IMG_UINT8* aui8Sl4x4IntraCb,
    IMG_UINT8* aui8Sl4x4IntraCr,
    IMG_UINT8* aui8Sl4x4InterY,
    IMG_UINT8* aui8Sl4x4InterCb,
    IMG_UINT8* aui8Sl4x4InterCr,
    IMG_UINT8* aui8Sl8x8IntraY,
    IMG_UINT8* aui8Sl8x8InterY)
{
    IMG_UINT8  *pui8QuantMem;
    IMG_UINT32 *pui32QuantReg;
    IMG_UINT8  *apui8QuantTables[8];
    IMG_UINT32  ui32Table, ui32Val;
	psb_buffer_p pCustomBuf = &(ctx->ctx_mem[ctx->ui32StreamID].bufs_custom_quant);
	IMG_UINT32  custom_quant_size = ctx->ctx_mem_size.custom_quant;


    // Scanning order for coefficients, see section 8.5.5 of H.264 specification
    // Note that even for interlaced mode, hardware takes the scaling values as if frame zig-zag scanning were being used
    IMG_UINT8 aui8ZigZagScan4x4[16] = {
        0,  1,  5,  6,
        2,  4,  7,  12,
        3,  8,  11, 13,
        9,  10, 14, 15
    };
    IMG_UINT8 aui8ZigZagScan8x8[64] = {
        0,  1,  5,  6,  14, 15, 27, 28,
        2,  4,  7,  13, 16, 26, 29, 42,
        3,  8,  12, 17, 25, 30, 41, 43,
        9,  11, 18, 24, 31, 40, 44, 53,
        10, 19, 23, 32, 39, 45, 52, 54,
        20, 22, 33, 38, 46, 51, 55, 60,
        21, 34, 37, 47, 50, 56, 59, 61,
        35, 36, 48, 49, 57, 58, 62, 63
    };


    if (!ctx) {
        return VA_STATUS_ERROR_UNKNOWN;
    }

    if (!ctx->bCustomScaling) {
        return VA_STATUS_ERROR_UNKNOWN;
    }

    /* Copy quantization values (in header order) */
    pui8QuantMem = (IMG_UINT8*)(pCustomBuf);
    memcpy(pui8QuantMem, aui8Sl4x4IntraY, 16);
    memcpy(pui8QuantMem + 16, aui8Sl4x4IntraCb, 16);
    memcpy(pui8QuantMem + 32, aui8Sl4x4IntraCr, 16);
    memcpy(pui8QuantMem + 48, aui8Sl4x4InterY, 16);
    memcpy(pui8QuantMem + 64, aui8Sl4x4InterCb, 16);
    memcpy(pui8QuantMem + 80, aui8Sl4x4InterCr, 16);
    memcpy(pui8QuantMem + 96, aui8Sl8x8IntraY, 64);
    memcpy(pui8QuantMem + 160, aui8Sl8x8InterY, 64);

    /* Create quantization register values */

    /* Assign based on the order values are written to registers */
    apui8QuantTables[0] = aui8Sl4x4IntraY;
    apui8QuantTables[1] = aui8Sl4x4InterY;
    apui8QuantTables[2] = aui8Sl4x4IntraCb;
    apui8QuantTables[3] = aui8Sl4x4InterCb;
    apui8QuantTables[4] = aui8Sl4x4IntraCr;
    apui8QuantTables[5] = aui8Sl4x4InterCr;
    apui8QuantTables[6] = aui8Sl8x8IntraY;
    apui8QuantTables[7] = aui8Sl8x8InterY;

    /* H264COMP_CUSTOM_QUANT_SP register values "psCustomQuantRegs4x4Sp"*/
    pui8QuantMem = (IMG_UINT8*)(pCustomBuf + custom_quant_size);
    pui32QuantReg = (IMG_UINT32 *)pui8QuantMem;
    for (ui32Table = 0; ui32Table < 6; ui32Table++) {
        for (ui32Val = 0; ui32Val < 16; ui32Val += 4) {
            *pui32QuantReg = F_ENCODE(ptg__H264ES_CalcCustomQuantSp(0, ui32Val, apui8QuantTables[ui32Table][aui8ZigZagScan4x4[ui32Val]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_0)
                             | F_ENCODE(ptg__H264ES_CalcCustomQuantSp(0, ui32Val + 1, apui8QuantTables[ui32Table][aui8ZigZagScan4x4[ui32Val + 1]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_1);
            pui32QuantReg++;
            *pui32QuantReg = F_ENCODE(ptg__H264ES_CalcCustomQuantSp(0, ui32Val + 2, apui8QuantTables[ui32Table][aui8ZigZagScan4x4[ui32Val + 2]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_0)
                             | F_ENCODE(ptg__H264ES_CalcCustomQuantSp(0, ui32Val + 3, apui8QuantTables[ui32Table][aui8ZigZagScan4x4[ui32Val + 3]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_1);
            pui32QuantReg++;
        }
    }

    /*psCustomQuantRegs8x8Sp*/
    pui8QuantMem = (IMG_UINT8*)(pCustomBuf + custom_quant_size + custom_quant_size);
    pui32QuantReg = (IMG_UINT32 *)pui8QuantMem;
    for (; ui32Table < 8; ui32Table++) {
        for (ui32Val = 0; ui32Val < 64; ui32Val += 8) {
            *pui32QuantReg = F_ENCODE(ptg__H264ES_CalcCustomQuantSp(1, ((ui32Val & 24) >> 1), apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_0)
                             | F_ENCODE(ptg__H264ES_CalcCustomQuantSp(1, ((ui32Val & 24) >> 1) + 1, apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 1]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_1);
            pui32QuantReg++;
            *pui32QuantReg = F_ENCODE(ptg__H264ES_CalcCustomQuantSp(1, ((ui32Val & 24) >> 1) + 2, apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 2]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_0)
                             | F_ENCODE(ptg__H264ES_CalcCustomQuantSp(1, ((ui32Val & 24) >> 1) + 3, apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 3]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_1);
            pui32QuantReg++;
            *pui32QuantReg = F_ENCODE(ptg__H264ES_CalcCustomQuantSp(1, ((ui32Val & 24) >> 1), apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 4]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_0)
                             | F_ENCODE(ptg__H264ES_CalcCustomQuantSp(1, ((ui32Val & 24) >> 1) + 1, apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 5]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_1);
            pui32QuantReg++;
            *pui32QuantReg = F_ENCODE(ptg__H264ES_CalcCustomQuantSp(1, ((ui32Val & 24) >> 1) + 2, apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 6]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_0)
                             | F_ENCODE(ptg__H264ES_CalcCustomQuantSp(1, ((ui32Val & 24) >> 1) + 3, apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 7]]), TOPAZHP_CR_H264COMP_CUSTOM_QUANT_SP_1);
            pui32QuantReg++;
        }
    }

    /* H264COMP_CUSTOM_QUANT_Q register values "psCustomQuantRegs4x4Q" */
    pui8QuantMem = (IMG_UINT8*)(pCustomBuf + custom_quant_size + custom_quant_size + custom_quant_size);
    pui32QuantReg = (IMG_UINT32 *)pui8QuantMem;
    for (ui32Table = 0; ui32Table < 6; ui32Table++) {
        for (ui32Val = 0; ui32Val < 16; ui32Val += 4) {
            *pui32QuantReg = F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan4x4[ui32Val]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_0)
                             | F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan4x4[ui32Val + 1]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_1)
                             | F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan4x4[ui32Val + 2]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_2)
                             | F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan4x4[ui32Val + 3]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_3);
            pui32QuantReg++;
        }
    }

    /*psCustomQuantRegs8x8Q)*/
    pui8QuantMem = (IMG_UINT8*)(pCustomBuf + custom_quant_size + custom_quant_size + custom_quant_size + custom_quant_size);

    pui32QuantReg = (IMG_UINT32 *)pui8QuantMem;
    for (; ui32Table < 8; ui32Table++) {
        for (ui32Val = 0; ui32Val < 64; ui32Val += 8) {
            *pui32QuantReg = F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_0)
                             | F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 1]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_1)
                             | F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 2]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_2)
                             | F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 3]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_3);
            pui32QuantReg++;
            *pui32QuantReg = F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 4]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_0)
                             | F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 5]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_1)
                             | F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 6]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_2)
                             | F_ENCODE(apui8QuantTables[ui32Table][aui8ZigZagScan8x8[ui32Val + 7]], TOPAZHP_CR_H264COMP_CUSTOM_QUANT_Q_3);
            pui32QuantReg++;
        }
    }

    if (ctx->bPpsScaling)
        ctx->bInsertPicHeader = IMG_TRUE;

    return VA_STATUS_SUCCESS;
}


void ptg__picmgmt_custom_scaling(context_ENC_p ctx, IMG_UINT32 ui32FrameNum)
{
    if (ui32FrameNum % ctx->ui32PpsScalingCnt == 0) {
        // Swap inter and intra scaling lists on alternating picture parameter sets
        if (ui32FrameNum % (ctx->ui32PpsScalingCnt * 2) == 0) {
            ptg__set_custom_scaling_values(
                ctx,
                ctx->aui8CustomQuantParams4x4[0],
                ctx->aui8CustomQuantParams4x4[1],
                ctx->aui8CustomQuantParams4x4[2],
                ctx->aui8CustomQuantParams4x4[3],
                ctx->aui8CustomQuantParams4x4[4],
                ctx->aui8CustomQuantParams4x4[5],
                ctx->aui8CustomQuantParams8x8[0],
                ctx->aui8CustomQuantParams8x8[1]);
        } else {
            ptg__set_custom_scaling_values(
                ctx,
                ctx->aui8CustomQuantParams4x4[3],
                ctx->aui8CustomQuantParams4x4[4],
                ctx->aui8CustomQuantParams4x4[5],
                ctx->aui8CustomQuantParams4x4[0],
                ctx->aui8CustomQuantParams4x4[1],
                ctx->aui8CustomQuantParams4x4[2],
                ctx->aui8CustomQuantParams8x8[1],
                ctx->aui8CustomQuantParams8x8[0]);
        }
    }
}

/************************* MTX_CMDID_PROVIDE_BUFFER *************************/
IMG_UINT32 ptg_send_codedbuf(
    context_ENC_p ctx,
    IMG_UINT32 ui32SlotIndex)
{
    context_ENC_frame_buf *ps_buf = &(ctx->ctx_frame_buf);
    object_context_p obj_context_p = ctx->obj_context;
    object_buffer_p object_buffer  = ps_buf->coded_buf;
    ptg_cmdbuf_p cmdbuf = obj_context_p->ptg_cmdbuf;
    IMG_UINT32 frame_mem_index = 0;
    IMG_BUFFER_PARAMS  *psBufferParams = NULL;
#ifdef _TOPAZHP_PDUMP_PICMGMT_
    char *ptmp = NULL;
    void *pxxx = NULL;
#endif

    int i;
#ifdef _TOPAZHP_PDUMP_PICMGMT_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s slot 0 = %x\n", __FUNCTION__, ui32SlotIndex);
#endif

    if (cmdbuf->frame_mem_index >= COMM_CMD_FRAME_BUF_NUM) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: Error: frame_mem buffer index overflow\n", __FUNCTION__);
        cmdbuf->frame_mem_index = 0;
    }

    frame_mem_index = cmdbuf->frame_mem_index * cmdbuf->mem_size;
    psBufferParams = (IMG_BUFFER_PARAMS *)(cmdbuf->frame_mem_p + frame_mem_index);

#ifdef _TOPAZHP_PDUMP_PICMGMT_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s frame_idx = %x, frame_mem_p = 0x%08x, mem_size = %d\n", __FUNCTION__, cmdbuf->frame_mem_index, cmdbuf->frame_mem_p, cmdbuf->mem_size);
#endif

    /* Prepare ProvideBuffer data */
    psBufferParams->eBufferType = IMG_BUFFER_CODED;
    psBufferParams->sData.sCoded.ui8SlotNum = (IMG_UINT8)(ui32SlotIndex & 0xff);
    psBufferParams->sData.sCoded.ui32Size = object_buffer->size;

#ifdef _TOPAZHP_PDUMP_PICMGMT_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s slot 1 = %x\n", __FUNCTION__, ui32SlotIndex);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s buf 0 = 0x%08x, buf 1 = 0x%08x, 0x%08x\n", __FUNCTION__, &(psBufferParams->ui32PhysAddr), object_buffer, object_buffer->psb_buffer);
#endif

#ifdef _TOPAZHP_OLD_LIBVA_
    ptg_cmdbuf_set_phys(&(psBufferParams->ui32PhysAddr), 0,
        object_buffer->psb_buffer, 0, 0);
#else
    RELOC_FRAME_PARAMS_PTG(&(psBufferParams->ui32PhysAddr), ui32SlotIndex * object_buffer->size, object_buffer->psb_buffer);
#endif

#ifdef _TOPAZHP_PDUMP_PICMGMT_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s slot 2 = %x\n", __FUNCTION__, ui32SlotIndex);
#endif

    ptg_cmdbuf_insert_command_package(ctx->obj_context, ctx->ui32StreamID,
        MTX_CMDID_PROVIDE_BUFFER | MTX_CMDID_PRIORITY,
        &cmdbuf->frame_mem, frame_mem_index);
    ++(cmdbuf->frame_mem_index);

#ifdef _TOPAZHP_PDUMP_PICMGMT_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s end\n", __FUNCTION__);
#endif
    return  VA_STATUS_SUCCESS;
}

static VAStatus ptg__set_component_offsets(
    context_ENC_p ctx,
    object_surface_p obj_surface_p,
    IMG_FRAME * psFrame
)
{
    IMG_FORMAT eFormat;
    IMG_UINT16 ui16Width;
    IMG_UINT16 ui16Stride;
    IMG_UINT16 ui16PictureHeight;

#ifdef _TOPAZHP_PDUMP_PICMGMT_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s start\n", __FUNCTION__);
#endif

    if (!ctx)
        return VA_STATUS_ERROR_UNKNOWN;
    // if source slot is NULL then it's just a next portion of slices
    if (psFrame == IMG_NULL)
        return VA_STATUS_ERROR_UNKNOWN;

    eFormat = ctx->eFormat;
    ui16Width = obj_surface_p->width;
    ui16PictureHeight = obj_surface_p->height;
    ui16Stride = obj_surface_p->psb_surface->stride;
#ifdef _TOPAZHP_PDUMP_PICMGMT_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s eFormat = %d, w = %d, h = %d, stride = %d\n",
        __FUNCTION__, eFormat, ui16Width, ui16PictureHeight, ui16Stride);
#endif
    // 3 Components: Y, U, V
    // Y component is always at the beginning
    psFrame->i32YComponentOffset = 0;
    psFrame->ui16SrcYStride = ui16Stride;

    // Assume for now that field 0 comes first
    psFrame->i32Field0YOffset = 0;
    psFrame->i32Field0UOffset = 0;
    psFrame->i32Field0VOffset = 0;


    switch (eFormat) {
    case IMG_CODEC_YUV:
        psFrame->ui16SrcUVStride = ui16Stride / 2;              // ui16SrcUStride

        psFrame->i32UComponentOffset = ui16Stride * ui16PictureHeight;   // ui16SrcUBase
        psFrame->i32VComponentOffset = ui16Stride * ui16PictureHeight + (ui16Stride / 2) * (ui16PictureHeight / 2); // ui16SrcVBase
        break;

    case IMG_CODEC_PL8:
        psFrame->ui16SrcUVStride = ui16Stride / 2;              // ui16SrcUStride

        psFrame->i32UComponentOffset = 0;   // ui16SrcUBase
        psFrame->i32VComponentOffset = 0; // ui16SrcVBase
        break;

    case IMG_CODEC_PL12:
        psFrame->ui16SrcUVStride = ui16Stride;                         // ui16SrcUStride
        //FIXME
        psFrame->i32UComponentOffset = ui16Stride * ui16PictureHeight; // ui16SrcUBase
        psFrame->i32VComponentOffset = ui16Stride * ui16PictureHeight; // ui16SrcVBase
        break;

    case IMG_CODEC_YV12:    /* YV12 */
        psFrame->ui16SrcUVStride = ui16Stride / 2;              // ui16SrcUStride

        psFrame->i32UComponentOffset = ui16Stride * ui16PictureHeight + (ui16Stride / 2) * (ui16PictureHeight / 2);   // ui16SrcUBase
        psFrame->i32VComponentOffset = ui16Stride * ui16PictureHeight; // ui16SrcVBase
        break;

    case IMG_CODEC_IMC2:    /* IMC2 */
        psFrame->ui16SrcUVStride = ui16Stride;                  // ui16SrcUStride

        psFrame->i32UComponentOffset = ui16Stride * ui16PictureHeight + (ui16Stride / 2);   // ui16SrcUBase
        psFrame->i32VComponentOffset = ui16Stride * ui16PictureHeight; // ui16SrcVBase
        break;

    case IMG_CODEC_422_YUV:
        psFrame->ui16SrcUVStride = ui16Stride;          // ui16SrcUStride

        psFrame->i32UComponentOffset = ui16Stride * ui16PictureHeight;   // ui16SrcUBase
        psFrame->i32VComponentOffset = ui16Stride * ui16PictureHeight + (ui16Stride / 2) * ui16PictureHeight; // ui16SrcVBase
        break;

    case IMG_CODEC_422_YV12:        /* YV16 */
        psFrame->ui16SrcUVStride = ui16Stride;          // ui16SrcUStride

        psFrame->i32UComponentOffset = ui16Stride * ui16PictureHeight + (ui16Stride / 2) * ui16PictureHeight;   // ui16SrcUBase
        psFrame->i32VComponentOffset = ui16Stride * ui16PictureHeight; // ui16SrcVBase
        break;

    case IMG_CODEC_422_PL8:
        psFrame->ui16SrcUVStride = ui16Stride;          // ui16SrcUStride

        psFrame->i32UComponentOffset = 0;   // ui16SrcUBase
        psFrame->i32VComponentOffset = 0; // ui16SrcVBase
        break;

    case IMG_CODEC_422_IMC2:        /* IMC2 */
        psFrame->ui16SrcUVStride = ui16Stride * 2;                      // ui16SrcUStride

        psFrame->i32UComponentOffset = ui16Stride * ui16PictureHeight + (ui16Stride / 2);   // ui16SrcUBase
        psFrame->i32VComponentOffset = ui16Stride * ui16PictureHeight; // ui16SrcVBase
        break;

    case IMG_CODEC_422_PL12:
        psFrame->ui16SrcUVStride = ui16Stride * 2;                      // ui16SrcUStride

        psFrame->i32UComponentOffset = 0;   // ui16SrcUBase
        psFrame->i32VComponentOffset = 0; // ui16SrcVBase
        break;

    case IMG_CODEC_Y0UY1V_8888:
    case IMG_CODEC_Y0VY1U_8888:
    case IMG_CODEC_UY0VY1_8888:
    case IMG_CODEC_VY0UY1_8888:
        psFrame->ui16SrcUVStride = ui16Stride;                  // ui16SrcUStride

        psFrame->i32UComponentOffset = 0;   // ui16SrcUBase
        psFrame->i32VComponentOffset = 0; // ui16SrcVBase
        break;

    default:
        break;
    }

    if (ctx->bIsInterlaced) {
        if (ctx->bIsInterleaved) {
            switch (eFormat) {
            case IMG_CODEC_IMC2:
            case IMG_CODEC_422_IMC2:
                psFrame->i32VComponentOffset *= 2;
                psFrame->i32UComponentOffset = psFrame->i32VComponentOffset + (ui16Stride / 2);
                break;
            default:
                psFrame->i32UComponentOffset *= 2;
                psFrame->i32VComponentOffset *= 2;
                break;
            }

            psFrame->i32Field1YOffset = psFrame->i32Field0YOffset + psFrame->ui16SrcYStride;
            psFrame->i32Field1UOffset = psFrame->i32Field0UOffset + psFrame->ui16SrcUVStride;
            psFrame->i32Field1VOffset = psFrame->i32Field0VOffset + psFrame->ui16SrcUVStride;

            psFrame->ui16SrcYStride *= 2;                           // ui16SrcYStride
            psFrame->ui16SrcUVStride *= 2;                  // ui16SrcUStride

            if (!ctx->bTopFieldFirst)       {
                IMG_INT32 i32Temp;

                i32Temp = psFrame->i32Field1YOffset;
                psFrame->i32Field1YOffset = psFrame->i32Field0YOffset;
                psFrame->i32Field0YOffset = i32Temp;

                i32Temp = psFrame->i32Field1UOffset;
                psFrame->i32Field1UOffset = psFrame->i32Field0UOffset;
                psFrame->i32Field0UOffset = i32Temp;

                i32Temp = psFrame->i32Field1VOffset;
                psFrame->i32Field1VOffset = psFrame->i32Field0VOffset;
                psFrame->i32Field0VOffset = i32Temp;
            }
        } else {
            IMG_UINT32 ui32YFieldSize, ui32CFieldSize;

            switch (eFormat) {
            case IMG_CODEC_Y0UY1V_8888:
            case IMG_CODEC_UY0VY1_8888:
            case IMG_CODEC_Y0VY1U_8888:
            case IMG_CODEC_VY0UY1_8888:
                ui32YFieldSize = ui16PictureHeight * ui16Stride * 2;
                ui32CFieldSize = ui32YFieldSize;
                break;
            case IMG_CODEC_PL8:
                ui32YFieldSize = ui16PictureHeight * ui16Stride;
                ui32CFieldSize = ui16PictureHeight * ui16Stride / 4;
                break;
            case IMG_CODEC_PL12:
                ui32YFieldSize = ui16PictureHeight * ui16Stride;
                ui32CFieldSize = ui16PictureHeight * ui16Stride / 2;
                break;
            case IMG_CODEC_422_YUV:
            case IMG_CODEC_422_YV12:
            case IMG_CODEC_422_IMC2:
                ui32YFieldSize = ui16PictureHeight * ui16Stride * 2;
                ui32CFieldSize = ui32YFieldSize;
                break;
            case IMG_CODEC_422_PL8:
                ui32YFieldSize = ui16PictureHeight * ui16Stride;
                ui32CFieldSize = ui16PictureHeight * ui16Stride / 2;
                break;
            case IMG_CODEC_422_PL12:
                ui32YFieldSize = ui16PictureHeight * ui16Stride;
                ui32CFieldSize = ui32YFieldSize;
                break;
            default:
                ui32YFieldSize = ui16PictureHeight * ui16Stride * 3 / 2;
                ui32CFieldSize = ui32YFieldSize;
                break;
            }

            psFrame->i32Field1YOffset = ui32YFieldSize;
            psFrame->i32Field1UOffset = ui32CFieldSize;
            psFrame->i32Field1VOffset = ui32CFieldSize;
        }
    } else {
        psFrame->i32Field1YOffset = psFrame->i32Field0YOffset;
        psFrame->i32Field1UOffset = psFrame->i32Field0UOffset;
        psFrame->i32Field1VOffset = psFrame->i32Field0VOffset;
    }
#ifdef _TOPAZHP_PDUMP_PICMGMT_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s i32YComponentOffset = %d, i32UComponentOffset = %d, i32VComponentOffset = %d\n",
        __FUNCTION__, (int)(psFrame->i32YComponentOffset), (int)(psFrame->i32UComponentOffset), (int)(psFrame->i32VComponentOffset));
#endif
     return VA_STATUS_SUCCESS;
}

IMG_UINT32 ptg_send_source_frame(
    context_ENC_p ctx,
    IMG_UINT32 ui32SlotIndex,
    IMG_UINT32 ui32DisplayOrder)
{
    context_ENC_frame_buf *ps_buf = &(ctx->ctx_frame_buf);
    IMG_FRAME  sSrcFrame;
    IMG_FRAME  *psSrcFrame = &sSrcFrame;
    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;

    unsigned int frame_mem_index = 0;
    IMG_BUFFER_PARAMS  *psBufferParams = NULL;
    object_surface_p src_surface = ps_buf->src_surface;
    unsigned int srf_buf_offset = src_surface->psb_surface->buf.buffer_ofs;
#ifdef _TOPAZHP_PDUMP_PICMGMT_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s: start ui32SlotIndex = %d, ui32DisplayOrder = %d\n", __FUNCTION__, ui32SlotIndex, ui32DisplayOrder);
#endif
    if (cmdbuf->frame_mem_index >= COMM_CMD_FRAME_BUF_NUM) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: Error: frame_mem buffer index overflow\n", __FUNCTION__);
        cmdbuf->frame_mem_index = 0;
    }

    frame_mem_index = cmdbuf->frame_mem_index * cmdbuf->mem_size;
    psBufferParams = (IMG_BUFFER_PARAMS *)(cmdbuf->frame_mem_p + frame_mem_index);
    memset(psBufferParams, 0, sizeof(IMG_BUFFER_PARAMS));
    memset(psSrcFrame, 0, sizeof(IMG_FRAME));
    ptg__set_component_offsets(ctx, src_surface, psSrcFrame);

    // mark the appropriate slot as filled
//    ctx->ui32SourceSlotBuff[ui32SlotIndex] = IMG_TRUE;
    ctx->aui32SourceSlotPOC[ui32SlotIndex] = ctx->ui32FrameCount[ctx->ui32StreamID];
 
    /* Prepare ProvideBuffer data */
    psBufferParams->eBufferType = IMG_BUFFER_SOURCE;
    {
        psBufferParams->ui32PhysAddr = 0;
        psBufferParams->sData.sSource.ui8SlotNum = (IMG_UINT8)(ui32SlotIndex & 0xff);
//        psBufferParams->sData.sSource.ui8DisplayOrderNum = (IMG_UINT8)(ctx->ui32FrameCount & 0xff);
        psBufferParams->sData.sSource.ui8DisplayOrderNum = (IMG_UINT8)(ui32DisplayOrder & 0xff);
        psBufferParams->sData.sSource.ui32HostContext = (IMG_UINT32)ctx;

#ifdef _TOPAZHP_OLD_LIBVA_
        ptg_cmdbuf_set_phys(&(psBufferParams->sData.sSource.ui32PhysAddrYPlane_Field0), 0,
            &(src_surface->psb_surface->buf), 
            srf_buf_offset + psSrcFrame->i32YComponentOffset + psSrcFrame->i32Field0YOffset, 0);
        ptg_cmdbuf_set_phys(&(psBufferParams->sData.sSource.ui32PhysAddrUPlane_Field0), 0,
            &(src_surface->psb_surface->buf), 
            srf_buf_offset + psSrcFrame->i32UComponentOffset + psSrcFrame->i32Field0UOffset, 0);
        ptg_cmdbuf_set_phys(&(psBufferParams->sData.sSource.ui32PhysAddrVPlane_Field0), 0,
            &(src_surface->psb_surface->buf), 
            srf_buf_offset + psSrcFrame->i32VComponentOffset + psSrcFrame->i32Field0VOffset, 0);
    
        ptg_cmdbuf_set_phys(&(psBufferParams->sData.sSource.ui32PhysAddrYPlane_Field1), 0,
            &(src_surface->psb_surface->buf), 
            srf_buf_offset + psSrcFrame->i32YComponentOffset + psSrcFrame->i32Field1YOffset, 0);
        ptg_cmdbuf_set_phys(&(psBufferParams->sData.sSource.ui32PhysAddrUPlane_Field1), 0,
            &(src_surface->psb_surface->buf), 
            srf_buf_offset + psSrcFrame->i32UComponentOffset + psSrcFrame->i32Field1UOffset, 0);
        ptg_cmdbuf_set_phys(&(psBufferParams->sData.sSource.ui32PhysAddrVPlane_Field1), 0,
            &(src_surface->psb_surface->buf), 
            srf_buf_offset + psSrcFrame->i32VComponentOffset + psSrcFrame->i32Field1VOffset, 0);
#else
        RELOC_PICMGMT_PARAMS_PTG(&psBufferParams->sData.sSource.ui32PhysAddrYPlane_Field0,
                                 srf_buf_offset + psSrcFrame->i32YComponentOffset + psSrcFrame->i32Field0YOffset, &src_surface->psb_surface->buf);
        RELOC_PICMGMT_PARAMS_PTG(&psBufferParams->sData.sSource.ui32PhysAddrUPlane_Field0,
                                 srf_buf_offset + psSrcFrame->i32YComponentOffset + psSrcFrame->i32Field0UOffset, &src_surface->psb_surface->buf);
        RELOC_PICMGMT_PARAMS_PTG(&psBufferParams->sData.sSource.ui32PhysAddrVPlane_Field0,
                                 srf_buf_offset + psSrcFrame->i32YComponentOffset + psSrcFrame->i32Field0VOffset, &src_surface->psb_surface->buf);

        RELOC_PICMGMT_PARAMS_PTG(&psBufferParams->sData.sSource.ui32PhysAddrYPlane_Field1,
                                 srf_buf_offset + psSrcFrame->i32YComponentOffset + psSrcFrame->i32Field1YOffset, &src_surface->psb_surface->buf);
        RELOC_PICMGMT_PARAMS_PTG(&psBufferParams->sData.sSource.ui32PhysAddrUPlane_Field1,
                                 srf_buf_offset + psSrcFrame->i32YComponentOffset + psSrcFrame->i32Field1UOffset, &src_surface->psb_surface->buf);
        RELOC_PICMGMT_PARAMS_PTG(&psBufferParams->sData.sSource.ui32PhysAddrVPlane_Field1,
                                 srf_buf_offset + psSrcFrame->i32YComponentOffset + psSrcFrame->i32Field1VOffset, &src_surface->psb_surface->buf);
#endif
    }
#ifdef _TOPAZHP_PDUMP_PICMGMT_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s slot_idx = %d, frame_count = %d\n", __FUNCTION__, (int)(ui32SlotIndex), (int)(ctx->ui32FrameCount[ctx->ui32StreamID]));
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s: YPlane_Field0 = 0x%08x, UPlane_Field0 = 0x%08x, VPlane_Field0 = 0x%08x\n", __FUNCTION__, (unsigned int)(psBufferParams->sData.sSource.ui32PhysAddrYPlane_Field0), (unsigned int)(psBufferParams->sData.sSource.ui32PhysAddrUPlane_Field0), (unsigned int)(psBufferParams->sData.sSource.ui32PhysAddrVPlane_Field0));
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s: YPlane_Field1 = 0x%08x, UPlane_Field1 = 0x%08x, VPlane_Field1 = 0x%08x\n", __FUNCTION__, (unsigned int)(psBufferParams->sData.sSource.ui32PhysAddrYPlane_Field1), (unsigned int)(psBufferParams->sData.sSource.ui32PhysAddrUPlane_Field1), (unsigned int)(psBufferParams->sData.sSource.ui32PhysAddrVPlane_Field1));
#endif
    /* Send ProvideBuffer Command */
    ptg_cmdbuf_insert_command_package(ctx->obj_context, ctx->ui32StreamID,
        MTX_CMDID_PROVIDE_BUFFER | MTX_CMDID_PRIORITY,
        &cmdbuf->frame_mem, frame_mem_index);
    ++(cmdbuf->frame_mem_index);

    return 0;
}


IMG_UINT32 ptg_send_rec_frames(
    context_ENC_p ctx,
    IMG_UINT32 ui32SlotIndex,
    IMG_INT8   i8HeaderSlotNum,
    IMG_BOOL   bLongTerm)
{
    unsigned int srf_buf_offset;
    context_ENC_frame_buf *ps_buf = &(ctx->ctx_frame_buf);
    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;
    IMG_UINT32 uiSlotLen = ui32SlotIndex * cmdbuf->mem_size;
    object_surface_p rec_surface = ps_buf->rec_surface;
    IMG_BUFFER_PARAMS  *psBufferParams = (IMG_BUFFER_PARAMS *)
                                         (cmdbuf->frame_mem_p + uiSlotLen);
    memset(psBufferParams, 0, sizeof(IMG_BUFFER_PARAMS));

    psBufferParams->eBufferType = IMG_BUFFER_RECON;
#ifdef LTREFHEADER
    psBufferParams->sData.sRef.i8HeaderSlotNum = i8HeaderSlotNum;
#endif
    psBufferParams->sData.sRef.b8LongTerm = bLongTerm;

    srf_buf_offset = rec_surface->psb_surface->buf.buffer_ofs;
#ifdef _TOPAZHP_OLD_LIBVA_
    ptg_cmdbuf_set_phys(&(psBufferParams->ui32PhysAddr), 0,
            &(rec_surface->psb_surface->buf), srf_buf_offset, 0);
#else
    RELOC_PICMGMT_PARAMS_PTG(&psBufferParams->ui32PhysAddr,
                             srf_buf_offset, &rec_surface->psb_surface->buf);
#endif
    /* Send ProvideBuffer Command */
    ptg_cmdbuf_insert_command_package(ctx->obj_context, ctx->ui32StreamID,
                                      MTX_CMDID_PROVIDE_BUFFER | MTX_CMDID_PRIORITY,
                                      &cmdbuf->frame_mem, uiSlotLen);

    return 0;
}

IMG_UINT32 ptg_send_ref_frames(
    context_ENC_p ctx,
    IMG_UINT32    ui32SlotIndex,
    IMG_BOOL      bLongTerm)
{
    unsigned int srf_buf_offset;
    context_ENC_frame_buf *ps_buf = &(ctx->ctx_frame_buf);
    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;
    IMG_UINT32 uiSlotLen = ui32SlotIndex * cmdbuf->mem_size;
    IMG_BUFFER_PARAMS  *psBufferParams = (IMG_BUFFER_PARAMS *)
                                         (cmdbuf->frame_mem_p + uiSlotLen);
    memset(psBufferParams, 0, sizeof(IMG_BUFFER_PARAMS));
    object_surface_p ref_surface = ps_buf->ref_surface;

    psBufferParams->eBufferType = IMG_BUFFER_REF0;
    psBufferParams->sData.sRef.b8LongTerm = bLongTerm;

    srf_buf_offset = ref_surface->psb_surface->buf.buffer_ofs;
#ifdef _TOPAZHP_OLD_LIBVA_
    ptg_cmdbuf_set_phys(&(psBufferParams->ui32PhysAddr), 0,
            &(ref_surface->psb_surface->buf), srf_buf_offset, 0);
#else
    RELOC_PICMGMT_PARAMS_PTG(&psBufferParams->ui32PhysAddr,
                             srf_buf_offset, &ref_surface->psb_surface->buf);
#endif

    /* Send ProvideBuffer Command */
    ptg_cmdbuf_insert_command_package(ctx->obj_context, ctx->ui32StreamID,
                                      MTX_CMDID_PROVIDE_BUFFER | MTX_CMDID_PRIORITY,
                                      &cmdbuf->frame_mem, uiSlotLen);

    return VA_STATUS_SUCCESS;
}

void ptg__picmgmt_general(context_ENC_p ctx, IMG_UINT32 ui32FrameNum)
{
    ptg_set_next_ref_type(ctx, 0, IMG_INTRA_IDR);
    ptg_send_ref_frames(ctx, 0, 0);
    return ;
}

