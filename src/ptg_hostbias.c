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
#include "hwdefs/topaz_vlc_regs.h"
#include "hwdefs/topazhp_multicore_regs.h"
#include "hwdefs/topazhp_multicore_regs_old.h"
#include "psb_drv_video.h"
#include "ptg_cmdbuf.h"
#include "ptg_hostbias.h"
#include "psb_drv_debug.h"

#define UNINIT_PARAM 0xCDCDCDCD
#define SPE_ZERO_THRESHOLD	6
#define TOPAZHP_DEFAULT_bZeroDetectionDisable IMG_FALSE
#define TOPAZHP_DEFAULT_uZeroBlock4x4Threshold		6
#define TOPAZHP_DEFAULT_uZeroBlock8x8Threshold		4
#define TH_SKIP_IPE                      6
#define TH_INTER                           60
#define TH_INTER_QP                    10
#define TH_INTER_MAX_LEVEL      1500
#define TH_SKIP_SPE                     6
#define SPE_ZERO_THRESHOLD   6

#define TOPAZHP_DEFAULT_uTHInter                           TH_INTER
#define TOPAZHP_DEFAULT_uTHInterQP                      TH_INTER_QP
#define TOPAZHP_DEFAULT_uTHInterMaxLevel		     TH_INTER_MAX_LEVEL
#define TOPAZHP_DEFAULT_uTHSkipIPE                      TH_SKIP_IPE
#define TOPAZHP_DEFAULT_uTHSkipSPE                      TH_SKIP_SPE

#define MIN_32_REV 0x00030200
#define MAX_32_REV 0x00030299

// New MP4 Lambda table
static IMG_INT8 MPEG4_QPLAMBDA_MAP[31] = {
	0,  0,  1,  2,  3, 
	3,  4,  4,  5,  5,
	6,  6,  7,  7,  8,  
	8,  9,  9,  10, 10,
	11, 11, 11, 11, 12,
	12, 12, 12, 13, 13, 13 
};

static IMG_INT8 H263_QPLAMBDA_MAP[31] = {
 0, 0, 1, 1, 2,
 2, 3, 3, 4, 4,
 4, 5, 5, 5, 6,
 6, 6, 7, 7, 7,
 7, 8, 8, 8, 8,
 9, 9, 9, 9, 10,10 };

// new H.264 Lambda
static IMG_INT8 H264_QPLAMBDA_MAP_SAD[40] = {
    2, 2, 2, 2, 3, 3, 4, 4,
    4, 5, 5, 5, 5, 5, 6, 6,
    6, 7, 7, 7, 8, 8, 9, 11,
    13, 14, 15, 17, 20, 23, 27, 31,
    36, 41, 51, 62, 74, 79, 85, 91
};

static IMG_INT8 H264_QPLAMBDA_MAP_SATD_TABLES[][52] = {
    //table 0
    {
        8,  8,  8,  8,  8,  8,  8,
        8,  8,  8,  8,  8,  8,  8,
        8,  8,  8,  9,  9,     10, 10,
        11, 11, 12, 12, 13,     13, 14,
        14, 15, 15, 16, 17,     18, 20,
        21, 23, 25, 27, 30,     32, 38,
        44, 50, 56, 63, 67,     72, 77,
        82, 87, 92
    },
};

static IMG_INT32 H264_DIRECT_BIAS[27] = {
    24, 24, 24, 24, 24, 24, 24, 24, 36,
    48, 48, 60, 60, 72, 72, 84, 96, 108,
    200, 324, 384, 528, 672, 804, 924, 1044, 1104
};

static IMG_INT32 H264_INTRA8_SCALE[27] = {
    (234 + 8) >> 4, (231 + 8) >> 4,
    (226 + 8) >> 4, (221 + 8) >> 4,
    (217 + 8) >> 4, (213 + 8) >> 4,
    (210 + 8) >> 4, (207 + 8) >> 4,
    (204 + 8) >> 4, (202 + 8) >> 4,
    (200 + 8) >> 4, (199 + 8) >> 4,
    (197 + 8) >> 4, (197 + 8) >> 4,
    (196 + 8) >> 4, (196 + 8) >> 4,
    (197 + 8) >> 4, (197 + 8) >> 4,
    (198 + 8) >> 4, (200 + 8) >> 4,
    (202 + 8) >> 4, (204 + 8) >> 4,
    (207 + 8) >> 4, (210 + 8) >> 4,
    (213 + 8) >> 4, (217 + 8) >> 4,
    (217 + 8) >> 4
};

/***********************************************************************************
* Function Name : H264InterBias
* Inputs                                : ui8QP
* Outputs                               :
* Returns                               : IMG_INT16
* Description           : return the Inter Bias Value to use for the given QP
  ************************************************************************************/

static IMG_INT16 H264_InterIntraBias[27] = {
    20, 20, 20, 20, 20, 20, 50,
    100, 210, 420, 420, 445, 470,
    495, 520, 535, 550, 570, 715,
    860, 900, 1000, 1200, 1400,
    1600, 1800, 2000
};

static IMG_INT16 ptg__H264ES_inter_bias(IMG_INT8 i8QP)
{
    if (i8QP < 1) {
        i8QP = 1;
    }
    if (i8QP > 51) {
        i8QP = 51;
    }

    //return aui16InterBiasValues[i8QP-36];
    return H264_InterIntraBias[(i8QP+1)>>1];
}

/*****************************************************************************
 *  Function Name      : CalculateDCScaler
 *  Inputs             : iQP, bChroma
 *  Outputs            : iDCScaler
 *  Returns            : IMG_INT
 *  Description        : Calculates either the Luma or Chroma DC scaler from the quantization scaler
 *******************************************************************************/
IMG_INT
CalculateDCScaler(IMG_INT iQP, IMG_BOOL bChroma)
{
    IMG_INT     iDCScaler;
    if(!bChroma)
    {
        if (iQP > 0 && iQP < 5)
        {
            iDCScaler = 8;
        }
        else if (iQP > 4 &&     iQP     < 9)
        {
            iDCScaler = 2 * iQP;
        }
        else if (iQP > 8 &&     iQP     < 25)
        {
            iDCScaler = iQP + 8;
        }
        else
        {
            iDCScaler = 2 * iQP -16;
        }
    }
    else
    {
        if (iQP > 0 && iQP < 5)
        {
            iDCScaler = 8;
        }
        else if (iQP > 4 &&     iQP     < 25)
        {
            iDCScaler = (iQP + 13) / 2;
        }
        else
        {
            iDCScaler = iQP - 6;
        }
    }
    return iDCScaler;
}

/**************************************************************************************************
* Function:		MPEG4_GenerateBiasTables
* Description:	Genereate the bias tables for MPEG4
*
***************************************************************************************************/
void
ptg__MPEG4ES_generate_bias_tables(
	context_ENC_p ctx)
{
    IMG_INT16 n;
    IMG_INT16 iX;
    IMG_UINT32 ui32RegVal;
    IMG_UINT8 uiDCScaleL,uiDCScaleC,uiLambda;
    IMG_UINT32 uDirectVecBias,iInterMBBias,iIntra16Bias;
    IMG_BIAS_PARAMS *psBiasParams = &(ctx->sBiasParams);

    ctx->sBiasTables.ui32LritcCacheChunkConfig = F_ENCODE(ctx->uChunksPerMb, TOPAZHP_CR_CACHE_CHUNKS_PER_MB) |
						 F_ENCODE(ctx->uMaxChunks, TOPAZHP_CR_CACHE_CHUNKS_MAX) |
						 F_ENCODE(ctx->uMaxChunks - ctx->uPriorityChunks, TOPAZHP_CR_CACHE_CHUNKS_PRIORITY);

 
    for(n=31;n>=1;n--)
    {
	iX = n-12;
	if(iX < 0)
	{
	    iX = 0;
	}
	// Dont Write QP Values To ESB -- IPE will write these values
	// Update the quantization parameter which includes doing Lamda and the Chroma QP

	uiDCScaleL = CalculateDCScaler(n, IMG_FALSE);
	uiDCScaleC = CalculateDCScaler(n, IMG_TRUE);
	uiLambda = psBiasParams->uLambdaSAD ? psBiasParams->uLambdaSAD : MPEG4_QPLAMBDA_MAP[n - 1];

	ui32RegVal = uiDCScaleL;
	ui32RegVal |= (uiDCScaleC)<<8;
	ui32RegVal |= (uiLambda)<<16;
	ctx->sBiasTables.aui32LambdaBias[n] = ui32RegVal;
    }

    for(n=31;n>=1;n-=2)
    {
	if(psBiasParams->bRCEnable || psBiasParams->bRCBiases)
	{
	    uDirectVecBias = psBiasParams->uTHSkipIPE * uiLambda;
	    iInterMBBias    = psBiasParams->uTHInter * (n - psBiasParams->uTHInterQP);
	    if(iInterMBBias < 0) 
	    iInterMBBias	= 0;
	    if(iInterMBBias > psBiasParams->uTHInterMaxLevel)
		iInterMBBias	= psBiasParams->uTHInterMaxLevel;					
		iIntra16Bias = 0;
	    } else {
		uDirectVecBias  = psBiasParams->uIPESkipVecBias;
		iInterMBBias    = psBiasParams->iInterMBBias;
		iIntra16Bias    = psBiasParams->iIntra16Bias;
	    }

	    ctx->sBiasTables.aui32IntraBias[n] = iIntra16Bias;
	    ctx->sBiasTables.aui32InterBias_P[n] = iInterMBBias;
	    ctx->sBiasTables.aui32DirectBias_P[n] = uDirectVecBias;
    }

    if(psBiasParams->bRCEnable || psBiasParams->bRCBiases)
	ctx->sBiasTables.ui32SpeZeroThreshold = psBiasParams->uiSpeZeroThreshold;
    else
	ctx->sBiasTables.ui32SpeZeroThreshold = psBiasParams->uiSpeZeroThld;
}

/**************************************************************************************************
* Function:		H263_GenerateBiasTables
* Description:	Genereate the bias tables for H.263
*
***************************************************************************************************/
void
ptg__H263ES_generate_bias_tables(
	context_ENC_p ctx)
{
    IMG_INT16 n;
    IMG_INT16 iX;
    IMG_UINT32 ui32RegVal;
    IMG_UINT8 uiDCScaleL,uiDCScaleC,uiLambda;
    IMG_UINT32 uDirectVecBias,iInterMBBias,iIntra16Bias;
    IMG_BIAS_PARAMS * psBiasParams = &(ctx->sBiasParams);

    ctx->sBiasTables.ui32LritcCacheChunkConfig = F_ENCODE(ctx->uChunksPerMb, TOPAZHP_CR_CACHE_CHUNKS_PER_MB) |
						 F_ENCODE(ctx->uMaxChunks, TOPAZHP_CR_CACHE_CHUNKS_MAX) |
						 F_ENCODE(ctx->uMaxChunks - ctx->uPriorityChunks, TOPAZHP_CR_CACHE_CHUNKS_PRIORITY);

    for(n=31;n>=1;n--)
    {
	iX = n-12;
	if(iX < 0)
	{
	    iX = 0;
	}
	// Dont Write QP Values To ESB -- IPE will write these values
	// Update the quantization parameter which includes doing Lamda and the Chroma QP

	uiDCScaleL	= CalculateDCScaler(n, IMG_FALSE);
	uiDCScaleC	= CalculateDCScaler(n, IMG_TRUE);
	uiLambda	= psBiasParams->uLambdaSAD ? psBiasParams->uLambdaSAD : H263_QPLAMBDA_MAP[n - 1];

	ui32RegVal=uiDCScaleL;
	ui32RegVal |= (uiDCScaleC)<<8;
	ui32RegVal |= (uiLambda)<<16;

   	ctx->sBiasTables.aui32LambdaBias[n] = ui32RegVal;
    }

    for(n=31;n>=1;n-=2)
    {
	if(psBiasParams->bRCEnable || psBiasParams->bRCBiases)
	{
	    uDirectVecBias = psBiasParams->uTHSkipIPE * uiLambda;
	    iInterMBBias    = psBiasParams->uTHInter * (n - psBiasParams->uTHInterQP);
	    if(iInterMBBias < 0) 
	  	iInterMBBias	= 0;
	    if(iInterMBBias > psBiasParams->uTHInterMaxLevel)
		iInterMBBias	= psBiasParams->uTHInterMaxLevel;
		iIntra16Bias = 0;
	} else {
	    uDirectVecBias  = psBiasParams->uIPESkipVecBias;
	    iInterMBBias    = psBiasParams->iInterMBBias;
	    iIntra16Bias    = psBiasParams->iIntra16Bias;
	}

	ctx->sBiasTables.aui32IntraBias[n] = iIntra16Bias;
	ctx->sBiasTables.aui32InterBias_P[n] = iInterMBBias;
	ctx->sBiasTables.aui32DirectBias_P[n] = uDirectVecBias;
    }

    if(psBiasParams->bRCEnable || psBiasParams->bRCBiases)
	ctx->sBiasTables.ui32SpeZeroThreshold = psBiasParams->uiSpeZeroThreshold;
    else
	ctx->sBiasTables.ui32SpeZeroThreshold = psBiasParams->uiSpeZeroThld;
}

/**************************************************************************************************
* Function:             H264_GenerateBiasTables
* Description:  Generate the bias tables for H.264
*
***************************************************************************************************/
static void ptg__H264ES_generate_bias_tables(context_ENC_p ctx)
{
    IMG_INT32 n;
    IMG_UINT32 ui32RegVal;
    IMG_UINT32 iIntra16Bias, uiSpeZeroThld, uIntra8Scale, uDirectVecBias_P, iInterMBBias_P, uDirectVecBias_B, iInterMBBias_B;
    IMG_BIAS_PARAMS * psBiasParams = &(ctx->sBiasParams);

    IMG_BYTE PVR_QP_SCALE_CR[52] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
        28, 29, 29, 30, 31, 32, 32, 33, 34, 34, 35, 35, 36, 36, 37, 37,
        37, 38, 38, 38, 39, 39, 39, 39
    };

    ctx->sBiasTables.ui32LritcCacheChunkConfig = 
        F_ENCODE(ctx->uChunksPerMb, TOPAZHP_CR_CACHE_CHUNKS_PER_MB) |
        F_ENCODE(ctx->uMaxChunks, TOPAZHP_CR_CACHE_CHUNKS_MAX) |
        F_ENCODE(ctx->uMaxChunks - ctx->uPriorityChunks, TOPAZHP_CR_CACHE_CHUNKS_PRIORITY);

    uIntra8Scale = 0;
    for (n = 51; n >= 0; n--) {
        IMG_INT32  iX;
        IMG_UINT32 uiLambdaSAD, uiLambdaSATD;

        iX = n - 12;
        if (iX < 0) iX = 0;

        uiLambdaSAD  = H264_QPLAMBDA_MAP_SAD[iX];
        uiLambdaSATD = H264_QPLAMBDA_MAP_SATD_TABLES[psBiasParams->uLambdaSATDTable][n];

        if (psBiasParams->uLambdaSAD  != 0) uiLambdaSAD  = psBiasParams->uLambdaSAD;
        if (psBiasParams->uLambdaSATD != 0) uiLambdaSATD = psBiasParams->uLambdaSATD;

        // Dont Write QP Values To ESB -- IPE will write these values
        // Update the quantization parameter which includes doing Lamda and the Chroma QP
        ui32RegVal = PVR_QP_SCALE_CR[n];
        ui32RegVal |= (uiLambdaSATD) << 8; //SATD lambda
        ui32RegVal |= (uiLambdaSAD) << 16; //SAD lambda

        ctx->sBiasTables.aui32LambdaBias[n] = ui32RegVal;
    }

    for (n = 52; n >= 0; n -= 2) {
        IMG_INT8 qp = n;
        if (qp > 51) qp = 51;

        if (psBiasParams->bRCEnable || psBiasParams->bRCBiases) {
            iInterMBBias_P  = ptg__H264ES_inter_bias(qp);
            uDirectVecBias_P  = H264_DIRECT_BIAS[n/2];

            iInterMBBias_B  = iInterMBBias_P;
            uDirectVecBias_B  = uDirectVecBias_P  ;

            iIntra16Bias    = 0;
            uIntra8Scale    = H264_INTRA8_SCALE[n/2] - 8;

            if (psBiasParams->uDirectVecBias != UNINIT_PARAM)
                uDirectVecBias_B  = psBiasParams->uDirectVecBias;
            if (psBiasParams->iInterMBBiasB != UNINIT_PARAM)
                iInterMBBias_B    = psBiasParams->iInterMBBiasB;

            if (psBiasParams->uIPESkipVecBias != UNINIT_PARAM)
                uDirectVecBias_P  = psBiasParams->uIPESkipVecBias;
            if (psBiasParams->iInterMBBias != UNINIT_PARAM)
                iInterMBBias_P    = psBiasParams->iInterMBBias;

            if (psBiasParams->iIntra16Bias != UNINIT_PARAM) iIntra16Bias    = psBiasParams->iIntra16Bias;
        } else {
            if (psBiasParams->uDirectVecBias == UNINIT_PARAM || psBiasParams->iInterMBBiasB == UNINIT_PARAM) {
                drv_debug_msg(VIDEO_DEBUG_GENERAL, "ERROR: Bias for B pictures not set up ... uDirectVecBias = 0x%x, iInterMBBiasB = 0x%x\n", psBiasParams->uDirectVecBias, psBiasParams->iInterMBBiasB);
                abort();
            }
            uDirectVecBias_B  = psBiasParams->uDirectVecBias;
            iInterMBBias_B    = psBiasParams->iInterMBBiasB;

            if (psBiasParams->uIPESkipVecBias == UNINIT_PARAM || psBiasParams->iInterMBBias == UNINIT_PARAM) {
                drv_debug_msg(VIDEO_DEBUG_GENERAL, "ERROR: Bias for I/P pictures not set up ... uIPESkipVecBias = 0x%x, iInterMBBias = 0x%x\n", psBiasParams->uIPESkipVecBias, psBiasParams->iInterMBBias);
                abort();
            }
            uDirectVecBias_P  = psBiasParams->uIPESkipVecBias;
            iInterMBBias_P    = psBiasParams->iInterMBBias;

            iIntra16Bias    = psBiasParams->iIntra16Bias;
            uiSpeZeroThld   = psBiasParams->uiSpeZeroThld;
        }

#ifdef BRN_30029
        //adjust the intra8x8 bias so that we don't do anything silly when 8x8 mode is not in use.
        if (ctx->ui32PredCombControl & F_ENCODE(1, TOPAZHP_CR_INTRA8X8_DISABLE)) {
            iIntra16Bias |= 0x7fff << 16;
        }
#endif
//      drv_debug_msg(VIDEO_DEBUG_GENERAL, "qp %d, iIntra16Bias %d, iInterMBBias %d, uDirectVecBias %d\n", qp, iIntra16Bias, iInterMBBias, uDirectVecBias);
        ctx->sBiasTables.aui32IntraBias[n] = iIntra16Bias;
        ctx->sBiasTables.aui32InterBias_P[n] = iInterMBBias_P;
        ctx->sBiasTables.aui32DirectBias_P[n] = uDirectVecBias_P;
        ctx->sBiasTables.aui32InterBias_B[n] = iInterMBBias_B;
        ctx->sBiasTables.aui32DirectBias_B[n] = uDirectVecBias_B;
        ctx->sBiasTables.aui32IntraScale[n] = uIntra8Scale;
    }

    if (psBiasParams->bRCEnable || psBiasParams->bRCBiases)
        ctx->sBiasTables.ui32SpeZeroThreshold = psBiasParams->uiSpeZeroThreshold;
    else
        ctx->sBiasTables.ui32SpeZeroThreshold = psBiasParams->uiSpeZeroThld;

    if (psBiasParams->bZeroDetectionDisable) {
        ctx->sBiasTables.ui32RejectThresholdH264 = F_ENCODE(0, TOPAZHP_CR_H264COMP_4X4_REJECT_THRSHLD)
                | F_ENCODE(0, TOPAZHP_CR_H264COMP_8X8_REJECT_THRSHLD);
    } else {
        ctx->sBiasTables.ui32RejectThresholdH264 = F_ENCODE(psBiasParams->uZeroBlock4x4Threshold, TOPAZHP_CR_H264COMP_4X4_REJECT_THRSHLD)
                | F_ENCODE(psBiasParams->uZeroBlock8x8Threshold, TOPAZHP_CR_H264COMP_8X8_REJECT_THRSHLD);
    }
}


/**************************************************************************************************
* Function:             VIDEO_GenerateBias
* Description:  Generate the bias tables
*
***************************************************************************************************/
VAStatus ptg__generate_bias(context_ENC_p ctx)
{
    assert(ctx);

    switch (ctx->eStandard) {
        case IMG_STANDARD_H264:
            ptg__H264ES_generate_bias_tables(ctx);
            break;
        case IMG_STANDARD_H263:
            ptg__H263ES_generate_bias_tables(ctx);
            break;
        case IMG_STANDARD_MPEG4:
	    ptg__MPEG4ES_generate_bias_tables(ctx);
	    break;
/*
        case IMG_STANDARD_MPEG2:
                MPEG2_GenerateBiasTables(ctx, psBiasParams);
                break;
*/
        default:
            break;
    }

    return VA_STATUS_SUCCESS;
}

//load bias
static IMG_INT H264_LAMBDA_COEFFS[4][3] = {
	{175, -10166, 163244 },	 //SATD Lambda High
	{ 16,   -236,   8693 },  //SATD Lambda Low
	{198, -12240, 198865 },	 //SAD Lambda High
	{ 12,   -176,   1402 },  //SAD Lambda Low
};

static IMG_INT MPEG4_LAMBDA_COEFFS[3] = {
	0, 458, 1030
};

static IMG_INT H263_LAMBDA_COEFFS[3] = {
	0, 333, 716
};

static void ptg__H263ES_load_bias_tables(
    context_ENC_p ctx,
    IMG_FRAME_TYPE eFrameType)
{
    IMG_INT32 n;
    IMG_UINT32 ui32Pipe,ui32RegVal;
    IMG_UINT32 count = 0, cmd_word = 0;
    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;
    IMG_BIAS_TABLES* psBiasTables = &(ctx->sBiasTables);
    IMG_UINT32 *pCount;

    cmd_word = (MTX_CMDID_SW_WRITEREG & MTX_CMDWORD_ID_MASK) << MTX_CMDWORD_ID_SHIFT;
    *cmdbuf->cmd_idx++ = cmd_word;
    pCount = cmdbuf->cmd_idx;
    cmdbuf->cmd_idx++;

    ctx->i32NumPipes = 1;
    ctx->ui32CoreRev = 0x00030401;

    for (ui32Pipe = 0; ui32Pipe < ctx->i32NumPipes; ui32Pipe++)
        ptg_cmdbuf_insert_reg_write(TOPAZ_CORE_REG, TOPAZHP_CR_SEQUENCER_CONFIG, 0, psBiasTables->ui32SeqConfigInit);

    if (ctx->ui32CoreRev <= MAX_32_REV)
    {
	for(n=31;n>=1;n--)
	{
	    //FIXME: Zhaohan, missing register TOPAZHP_TOP_CR_LAMBDA_DC_TABLE
	    //ptg_cmdbuf_insert_reg_write(TOPAZHP_TOP_CR_LAMBDA_DC_TABLE, 0, psBiasTables->aui32LambdaBias[n]);
	}
    } else {
	ui32RegVal = (((H263_LAMBDA_COEFFS[0]) << (SHIFT_TOPAZHP_TOP_CR_POLYNOM_ALPHA_COEFF_CORE_00)) & (MASK_TOPAZHP_TOP_CR_POLYNOM_ALPHA_COEFF_CORE_00));
	ui32RegVal |= (((H263_LAMBDA_COEFFS[1]) << (SHIFT_TOPAZHP_TOP_CR_POLYNOM_BETA_COEFF_CORE_00)) & (MASK_TOPAZHP_TOP_CR_POLYNOM_BETA_COEFF_CORE_00));
	ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_ALPHA_COEFF_CORE_0, 0, ui32RegVal);

	ui32RegVal = (((H263_LAMBDA_COEFFS[2]) << (SHIFT_TOPAZHP_TOP_CR_POLYNOM_GAMMA_COEFF_CORE_01)) & (MASK_TOPAZHP_TOP_CR_POLYNOM_GAMMA_COEFF_CORE_01));
	ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_GAMMA_COEFF_CORE_0, 0, ui32RegVal);

	ui32RegVal = 0x3f;
	ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_CUTOFF_CORE_0, 0, ui32RegVal);
    }

    for(n=31;n>=1;n-=2)
    {
	ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_INTRA_BIAS_TABLE, 0, psBiasTables->aui32IntraBias[n]);
	ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_INTER_BIAS_TABLE, 0, psBiasTables->aui32InterBias_P[n]);
	ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_DIRECT_BIAS_TABLE, 0, psBiasTables->aui32DirectBias_P[n]);
    }

    for (ui32Pipe = 0; ui32Pipe < ctx->i32NumPipes; ui32Pipe++) {
	ptg_cmdbuf_insert_reg_write(TOPAZ_CORE_REG, TOPAZHP_CR_SPE_ZERO_THRESH, 0, psBiasTables->ui32SpeZeroThreshold);
    }

    for (ui32Pipe = 0; ui32Pipe < ctx->i32NumPipes; ui32Pipe++)
        ptg_cmdbuf_insert_reg_write(TOPAZ_CORE_REG, TOPAZHP_CR_LRITC_CACHE_CHUNK_CONFIG, 0, psBiasTables->ui32LritcCacheChunkConfig);

    *pCount = count;
}

static void ptg__MPEG4_load_bias_tables(context_ENC_p ctx)
{
    IMG_INT32 n;
    IMG_UINT32 ui32Pipe, ui32RegVal;
    IMG_UINT32 count = 0, cmd_word = 0;
    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;
    IMG_BIAS_TABLES* psBiasTables = &(ctx->sBiasTables);
    IMG_UINT32 *pCount;

    cmd_word = (MTX_CMDID_SW_WRITEREG & MTX_CMDWORD_ID_MASK) << MTX_CMDWORD_ID_SHIFT;
    *cmdbuf->cmd_idx++ = cmd_word;
    pCount = cmdbuf->cmd_idx;
    cmdbuf->cmd_idx++;

    ctx->i32NumPipes = 1;
    ctx->ui32CoreRev = 0x00030401;

    for (ui32Pipe = 0; ui32Pipe < ctx->i32NumPipes; ui32Pipe++)
        ptg_cmdbuf_insert_reg_write(TOPAZ_CORE_REG, TOPAZHP_CR_SEQUENCER_CONFIG, 0, psBiasTables->ui32SeqConfigInit);

    if (ctx->ui32CoreRev <= MAX_32_REV) {
        for (n=31; n >= 1; n--) {
	    //FIXME: Zhaohan, missing register TOPAZHP_TOP_CR_LAMBDA_DC_TABLE
            //ptg_cmdbuf_insert_reg_write(TOPAZHP_TOP_CR_LAMBDA_DC_TABLE, 0, psBiasTables->aui32LambdaBias[n]);
        }
    } else {
	    //ui32RegVal = MPEG4_LAMBDA_COEFFS[0]| (MPEG4_LAMBDA_COEFFS[1]<<8);
	    ui32RegVal = (((MPEG4_LAMBDA_COEFFS[0]) << (SHIFT_TOPAZHP_TOP_CR_POLYNOM_ALPHA_COEFF_CORE_00)) & (MASK_TOPAZHP_TOP_CR_POLYNOM_ALPHA_COEFF_CORE_00));
	    ui32RegVal |= (((MPEG4_LAMBDA_COEFFS[1]) << (SHIFT_TOPAZHP_TOP_CR_POLYNOM_BETA_COEFF_CORE_00)) & (MASK_TOPAZHP_TOP_CR_POLYNOM_BETA_COEFF_CORE_00));

	    ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_ALPHA_COEFF_CORE_0, 0, ui32RegVal);
	    //ui32RegVal = MPEG4_LAMBDA_COEFFS[2];
	    ui32RegVal = (((MPEG4_LAMBDA_COEFFS[2]) << (SHIFT_TOPAZHP_TOP_CR_POLYNOM_GAMMA_COEFF_CORE_01)) & (MASK_TOPAZHP_TOP_CR_POLYNOM_GAMMA_COEFF_CORE_01));

	    ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_GAMMA_COEFF_CORE_0, 0, ui32RegVal);	
	
	    ui32RegVal = 0x3f;
	    ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_CUTOFF_CORE_0, 0, ui32RegVal);
    }

    for(n=31;n>=1;n-=2)
    {
	ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_INTRA_BIAS_TABLE, 0, psBiasTables->aui32IntraBias[n]);
	ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_INTER_BIAS_TABLE, 0, psBiasTables->aui32InterBias_P[n]);
	ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_DIRECT_BIAS_TABLE, 0, psBiasTables->aui32DirectBias_P[n]);
    }

    for (ui32Pipe = 0; ui32Pipe < ctx->i32NumPipes; ui32Pipe++) {
	ptg_cmdbuf_insert_reg_write(TOPAZ_CORE_REG, TOPAZHP_CR_SPE_ZERO_THRESH, 0, psBiasTables->ui32SpeZeroThreshold);
		
	//VLC RSize is fcode - 1 and only done for mpeg4 AND mpeg2 not H263
	ptg_cmdbuf_insert_reg_write(TOPAZ_VLC_REG, TOPAZ_VLC_CR_VLC_MPEG4_CFG, 0, F_ENCODE(psBiasTables->ui32FCode - 1, TOPAZ_VLC_CR_RSIZE));
    }

    for (ui32Pipe = 0; ui32Pipe < ctx->i32NumPipes; ui32Pipe++)
        ptg_cmdbuf_insert_reg_write(TOPAZ_CORE_REG, TOPAZHP_CR_LRITC_CACHE_CHUNK_CONFIG, 0, psBiasTables->ui32LritcCacheChunkConfig);

    *pCount = count;
}

static void ptg__H264ES_load_bias_tables(
    context_ENC_p ctx,
    IMG_FRAME_TYPE eFrameType)
{
    IMG_INT32 n;
    IMG_UINT32 ui32Pipe;
    IMG_UINT32 ui32RegVal;
    IMG_BIAS_TABLES* psBiasTables = &(ctx->sBiasTables);
    ptg_cmdbuf_p cmdbuf = ctx->obj_context->ptg_cmdbuf;
    IMG_UINT32 count = 0, cmd_word = 0;
    IMG_UINT32 *pCount;
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s: start\n", __FUNCTION__);
#endif

    cmd_word = (MTX_CMDID_SW_WRITEREG & MTX_CMDWORD_ID_MASK) << MTX_CMDWORD_ID_SHIFT;
    *cmdbuf->cmd_idx++ = cmd_word;
    pCount = cmdbuf->cmd_idx;
    cmdbuf->cmd_idx++;

    ctx->i32NumPipes = 1;
    psBiasTables->ui32SeqConfigInit = 0x40038412;

    for (ui32Pipe = 0; ui32Pipe < ctx->i32NumPipes; ui32Pipe++)
        ptg_cmdbuf_insert_reg_write(TOPAZ_CORE_REG, TOPAZHP_CR_SEQUENCER_CONFIG, 0, psBiasTables->ui32SeqConfigInit);

    ctx->ui32CoreRev = 0x00030401;
    
    if (ctx->ui32CoreRev <= MAX_32_REV) {
        for (n=51; n >= 0; n--) {
	    //FIXME: Zhaohan, missing register TOPAZHP_TOP_CR_LAMBDA_DC_TABLE
            //ptg_cmdbuf_insert_reg_write(TOPAZHP_TOP_CR_LAMBDA_DC_TABLE, 0, psBiasTables->aui32LambdaBias[n]);
        }
    } else {
        //Load the lambda coeffs
        for (n = 0; n < 4; n++) {
            //ui32RegVal = H264_LAMBDA_COEFFS[n][0]| (H264_LAMBDA_COEFFS[n][1]<<8);
            ui32RegVal = (((H264_LAMBDA_COEFFS[n][0]) << (SHIFT_TOPAZHP_TOP_CR_POLYNOM_ALPHA_COEFF_CORE_00)) & (MASK_TOPAZHP_TOP_CR_POLYNOM_ALPHA_COEFF_CORE_00));
            ui32RegVal |= (((H264_LAMBDA_COEFFS[n][1]) << (SHIFT_TOPAZHP_TOP_CR_POLYNOM_BETA_COEFF_CORE_00)) & (MASK_TOPAZHP_TOP_CR_POLYNOM_BETA_COEFF_CORE_00));

            ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_ALPHA_COEFF_CORE_0, 0, ui32RegVal);
            ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_ALPHA_COEFF_CORE_1, 0, ui32RegVal);
            ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_ALPHA_COEFF_CORE_2, 0, ui32RegVal);

            //ui32RegVal = H264_LAMBDA_COEFFS[n][2];
            ui32RegVal = (((H264_LAMBDA_COEFFS[n][2]) << (SHIFT_TOPAZHP_TOP_CR_POLYNOM_GAMMA_COEFF_CORE_01)) & (MASK_TOPAZHP_TOP_CR_POLYNOM_GAMMA_COEFF_CORE_01));

            ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_GAMMA_COEFF_CORE_0, 0, ui32RegVal);
            ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_GAMMA_COEFF_CORE_1, 0, ui32RegVal);
            ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_GAMMA_COEFF_CORE_2, 0, ui32RegVal);
        }
        ui32RegVal = 29 |(29<<6);
        ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_CUTOFF_CORE_0, 0, ui32RegVal);
        ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_CUTOFF_CORE_1, 0, ui32RegVal);
        ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_POLYNOM_CUTOFF_CORE_2, 0, ui32RegVal);
    }

    for (n=52;n>=0;n-=2) {
        if (eFrameType == IMG_INTER_B) {
            ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_INTER_BIAS_TABLE, 0, psBiasTables->aui32InterBias_B[n]);
            ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_DIRECT_BIAS_TABLE, 0, psBiasTables->aui32DirectBias_B[n]);
        } else {
            ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_INTER_BIAS_TABLE, 0, psBiasTables->aui32InterBias_P[n]);
            ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_DIRECT_BIAS_TABLE, 0, psBiasTables->aui32DirectBias_P[n]);
        }
        ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_INTRA_BIAS_TABLE, 0, psBiasTables->aui32IntraBias[n]);
        ptg_cmdbuf_insert_reg_write(TOPAZ_MULTICORE_REG, TOPAZHP_TOP_CR_INTRA_SCALE_TABLE, 0, psBiasTables->aui32IntraScale[n]);
    }

    //aui32HpCoreRegId[ui32Pipe]
    for (ui32Pipe = 0; ui32Pipe < ctx->i32NumPipes; ui32Pipe++) {
        ptg_cmdbuf_insert_reg_write(TOPAZ_CORE_REG, TOPAZHP_CR_SPE_ZERO_THRESH, 0, psBiasTables->ui32SpeZeroThreshold);
        ptg_cmdbuf_insert_reg_write(TOPAZ_CORE_REG, TOPAZHP_CR_H264COMP_REJECT_THRESHOLD, 0, psBiasTables->ui32RejectThresholdH264);
    }

//    ptg_cmdbuf_insert_reg_write(TOPAZHP_TOP_CR_FIRMWARE_REG_1, (MTX_SCRATCHREG_TOMTX<<2), ui32BuffersReg);
//    ptg_cmdbuf_insert_reg_write(TOPAZHP_TOP_CR_FIRMWARE_REG_1, (MTX_SCRATCHREG_TOHOST<<2),ui32ToHostReg);

    // now setup the LRITC chache priority
    {
        //aui32HpCoreRegId[ui32Pipe]
        for (ui32Pipe = 0; ui32Pipe < ctx->i32NumPipes; ui32Pipe++) {
            ptg_cmdbuf_insert_reg_write(TOPAZ_CORE_REG, TOPAZHP_CR_LRITC_CACHE_CHUNK_CONFIG, 0, psBiasTables->ui32LritcCacheChunkConfig);
        }
    }
#ifdef _PDUMP_FUNC_
    drv_debug_msg(VIDEO_DEBUG_GENERAL, ("%s: count = %d\n", __FUNCTION__, (int)count);
#endif

    *pCount = count;
}

VAStatus ptg_load_bias(context_ENC_p ctx, IMG_FRAME_TYPE eFrameType)
{
    IMG_STANDARD eStandard = ctx->eStandard;

    switch (eStandard) {
        case IMG_STANDARD_H264:
            ptg__H264ES_load_bias_tables(ctx, eFrameType); //IMG_INTER_P);
            break;
        case IMG_STANDARD_H263:
            ptg__H263ES_load_bias_tables(ctx, eFrameType); //IMG_INTER_P);
            break;
        case IMG_STANDARD_MPEG4:
            ptg__MPEG4_load_bias_tables(ctx);
            break;
/*
        case IMG_STANDARD_MPEG2:
            ptg__MPEG2_LoadBiasTables(psBiasTables);
            break;
*/
        default:
            break;
    }

    return VA_STATUS_SUCCESS;
}

void ptg_init_bias_params(context_ENC_p ctx)
{
    IMG_BIAS_PARAMS * psBiasParams = &(ctx->sBiasParams);
    memset(psBiasParams, 0, sizeof(IMG_BIAS_PARAMS));
    //default
    psBiasParams->uLambdaSAD = 0;   
    psBiasParams->uLambdaSATD = 0;
    psBiasParams->uLambdaSATDTable = 0;

    psBiasParams->bRCEnable = ctx->sRCParams.bRCEnable;
    psBiasParams->bRCBiases = IMG_TRUE;

    psBiasParams->iIntra16Bias = UNINIT_PARAM;
    psBiasParams->iInterMBBias = UNINIT_PARAM;
    psBiasParams->iInterMBBiasB = UNINIT_PARAM;

    psBiasParams->uDirectVecBias = UNINIT_PARAM;
    psBiasParams->uIPESkipVecBias = UNINIT_PARAM;
    psBiasParams->uSPESkipVecBias = 0;      //not in spec

    psBiasParams->uiSpeZeroThreshold = SPE_ZERO_THRESHOLD;
    psBiasParams->uiSpeZeroThld = SPE_ZERO_THRESHOLD;
    psBiasParams->bZeroDetectionDisable = TOPAZHP_DEFAULT_bZeroDetectionDisable;

    psBiasParams->uZeroBlock4x4Threshold = TOPAZHP_DEFAULT_uZeroBlock4x4Threshold;
    psBiasParams->uZeroBlock8x8Threshold = TOPAZHP_DEFAULT_uZeroBlock8x8Threshold;

    psBiasParams->uTHInter = TOPAZHP_DEFAULT_uTHInter;
    psBiasParams->uTHInterQP = TOPAZHP_DEFAULT_uTHInterQP;
    psBiasParams->uTHInterMaxLevel = TOPAZHP_DEFAULT_uTHInterMaxLevel;
    psBiasParams->uTHSkipIPE = TOPAZHP_DEFAULT_uTHSkipIPE;
    psBiasParams->uTHSkipSPE = TOPAZHP_DEFAULT_uTHSkipSPE;
}
