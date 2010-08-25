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

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "psb_def.h"
#include "psb_surface.h"
#include "psb_cmdbuf.h"
#include "pnw_hostcode.h"
#include "pnw_H264ES.h"
#include "pnw_hostheader.h"

#define TOPAZ_H264_MAX_BITRATE 50000000

#define INIT_CONTEXT_H264ES	context_ENC_p ctx = (context_ENC_p) obj_context->format_data
#define SURFACE(id)    ((object_surface_p) object_heap_lookup( &ctx->obj_context->driver_data->surface_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &ctx->obj_context->driver_data->buffer_heap, id ))


static void pnw_H264ES_QueryConfigAttributes(
    VAProfile profile,
    VAEntrypoint entrypoint,
    VAConfigAttrib *attrib_list,
    int num_attribs )
{
    int i;

    psb__information_message("pnw_H264ES_QueryConfigAttributes\n");

    /* RateControl attributes */
    for (i = 0; i < num_attribs; i++) {
        switch (attrib_list[i].type) {
        case VAConfigAttribRTFormat:
            break;
                
        case VAConfigAttribRateControl:
            attrib_list[i].value = VA_RC_NONE | VA_RC_CBR | VA_RC_VBR;
            break;
              
        default:
            attrib_list[i].value = VA_ATTRIB_NOT_SUPPORTED;
            break;
        }
    }
}


static VAStatus pnw_H264ES_ValidateConfig(
    object_config_p obj_config )
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    psb__information_message("pnw_H264ES_ValidateConfig\n");

    return vaStatus;

}


static VAStatus pnw_H264ES_CreateContext(
    object_context_p obj_context,
    object_config_p obj_config )
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_ENC_p ctx;
    int i;
    unsigned int eRCmode;

    psb__information_message("pnw_H264ES_CreateContext\n");

    vaStatus = pnw_CreateContext(obj_context,obj_config, 0);

    if(VA_STATUS_SUCCESS != vaStatus) 
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    
    ctx = (context_ENC_p) obj_context->format_data;

    for(i = 0; i < obj_config->attrib_count; i++) {
        if(obj_config->attrib_list[i].type == VAConfigAttribRateControl)
            break;
    }

    if(i >= obj_config->attrib_count)
        eRCmode = VA_RC_NONE;
    else
        eRCmode = obj_config->attrib_list[i].value;


    if( eRCmode == VA_RC_VBR) {
        ctx->eCodec = IMG_CODEC_H264_VBR;
        ctx->sRCParams.RCEnable = IMG_TRUE;
    }
    else if ( eRCmode == VA_RC_CBR) {
        ctx->eCodec = IMG_CODEC_H264_CBR;
        ctx->sRCParams.RCEnable = IMG_TRUE;
    }
    else if ( eRCmode == VA_RC_NONE) {
        ctx->eCodec = IMG_CODEC_H264_NO_RC;
        ctx->sRCParams.RCEnable = IMG_FALSE;
    }
    else
        return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
    ctx->eFormat = IMG_CODEC_PL12;	/* use default */

    ctx->Slices = 1;

    if (getenv("PSB_VIDEO_DUAL_CORE") != NULL)
    {
	ctx->Slices = 2;
	ctx->NumCores = 2;
    }

    ctx->ParallelCores = min(ctx->NumCores, ctx->Slices);

    ctx->IPEControl = pnw__get_ipe_control(ctx->eCodec);

    switch (obj_config->profile)
    {
        case VAProfileH264Baseline:
            ctx->profile_idc = 5;
            break;
        case VAProfileH264Main:
            ctx->profile_idc = 6;
            break;
        default:
            ctx->profile_idc = 6; 
            break;
    }
    
    return vaStatus;
}


static void pnw_H264ES_DestroyContext(
    object_context_p obj_context)
{
    psb__information_message("pnw_H264ES_DestroyPicture\n");

    pnw_DestroyContext(obj_context);
}

static VAStatus pnw_H264ES_BeginPicture(
    object_context_p obj_context)
{
    INIT_CONTEXT_H264ES;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    psb__information_message("pnw_H264ES_BeginPicture\n");

    vaStatus = pnw_BeginPicture(ctx);

    return vaStatus;
}

static VAStatus pnw__H264ES_process_sequence_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAEncSequenceParameterBufferH264 *pSequenceParams;
    pnw_cmdbuf_p cmdbuf=ctx->obj_context->pnw_cmdbuf;
    H264_VUI_PARAMS VUI_Params;
    H264_CROP_PARAMS sCrop;
    int i;

    ASSERT(obj_buffer->type == VAEncSequenceParameterBufferType);
    ASSERT(obj_buffer->num_elements == 1);
    ASSERT(obj_buffer->size == sizeof(VAEncSequenceParameterBufferH264));

    if ((obj_buffer->num_elements != 1) ||
        (obj_buffer->size != sizeof(VAEncSequenceParameterBufferH264) ) )
    {
        return VA_STATUS_ERROR_UNKNOWN;
    }
    
    pSequenceParams = (VAEncSequenceParameterBufferH264 *) obj_buffer->buffer_data;
    obj_buffer->buffer_data = NULL;
    obj_buffer->size = 0;

    if (pSequenceParams->bits_per_second > TOPAZ_H264_MAX_BITRATE)
    {
	ctx->sRCParams.BitsPerSecond = TOPAZ_H264_MAX_BITRATE;
	psb__information_message(" bits_per_second(%d) exceeds \
		the maximum bitrate, set it with %d\n", 
		pSequenceParams->bits_per_second,
		TOPAZ_H264_MAX_BITRATE);
    }
    else
	ctx->sRCParams.BitsPerSecond = pSequenceParams->bits_per_second;

    ctx->sRCParams.FrameRate = pSequenceParams->frame_rate;
    ctx->sRCParams.InitialQp = pSequenceParams->initial_qp;
    ctx->sRCParams.MinQP = pSequenceParams->min_qp;
    ctx->sRCParams.BUSize = pSequenceParams->basic_unit_size;
	
    ctx->sRCParams.Slices = ctx->Slices; 
    ctx->sRCParams.QCPOffset = 0;/* FIXME */
    ctx->sRCParams.IntraFreq = pSequenceParams->intra_period;

    /*if (ctx->sRCParams.BitsPerSecond < 256000)
        ctx->sRCParams.BufferSize = (9 * ctx->sRCParams.BitsPerSecond) >> 1;
    else
        ctx->sRCParams.BufferSize = (5 * ctx->sRCParams.BitsPerSecond) >> 1;*/

    ctx->sRCParams.BufferSize = ctx->sRCParams.BitsPerSecond;
    ctx->sRCParams.InitialLevel = (3 * ctx->sRCParams.BufferSize) >> 4;
    ctx->sRCParams.InitialDelay = (13 * ctx->sRCParams.BufferSize) >> 4;
   
    if(ctx->obj_context->frame_count == 0)/* Add Register IO behind begin Picture */ 
    {
	pnw__UpdateRCBitsTransmitted(ctx);
	for(i = (ctx->ParallelCores - 1); i >= 0; i--) {
	    pnw_set_bias(ctx, i);
	}
	/* This is needed in old firmware, now is no needed any more */
	//ctx->initial_qp_in_cmdbuf = cmdbuf->cmd_idx; /* remember the place */
	//cmdbuf->cmd_idx++;
    }

    VUI_Params.Time_Scale = ctx->sRCParams.FrameRate*2;
    VUI_Params.bit_rate_value_minus1 = ctx->sRCParams.BitsPerSecond/64 -1;
    VUI_Params.cbp_size_value_minus1 = ctx->sRCParams.BitsPerSecond*3/32 -1;
    VUI_Params.CBR = 1;
    VUI_Params.initial_cpb_removal_delay_length_minus1 = 0;
    VUI_Params.cpb_removal_delay_length_minus1 = 0;
    VUI_Params.dpb_output_delay_length_minus1 = 0;
    VUI_Params.time_offset_length = 0;

    sCrop.bClip = IMG_FALSE;
    sCrop.LeftCropOffset = 0;
    sCrop.RightCropOffset = 0;
    sCrop.TopCropOffset = 0;
    sCrop.BottomCropOffset = 0;

    if (ctx->RawHeight & 0xf)
    {
        sCrop.bClip = IMG_TRUE;
        sCrop.BottomCropOffset = (((ctx->RawHeight + 0xf) & (~0xf)) - ctx->RawHeight)/2;
    }
    if (ctx->RawWidth & 0xf)
    {
        sCrop.bClip = IMG_TRUE;
        sCrop.RightCropOffset = (((ctx->RawWidth + 0xf) & (~0xf)) - ctx->RawWidth)/2;
    }
    /* sequence header is always inserted */

    /* Not know why set VUI_Params.CBR = 0; */    
    /*
    if (ctx->eCodec== IMG_CODEC_H264_NO_RC)
	    VUI_Params.CBR = 0;
    */
    memset(cmdbuf->header_mem_p + ctx->seq_header_ofs,
	    0,
	    HEADER_SIZE); 

    if (ctx->eCodec== IMG_CODEC_H264_NO_RC)
	pnw__H264_prepare_sequence_header(cmdbuf->header_mem_p + ctx->seq_header_ofs,
		pSequenceParams->picture_width_in_mbs,
		pSequenceParams->picture_height_in_mbs,IMG_FALSE,&VUI_Params,&sCrop,
		pSequenceParams->level_idc, ctx->profile_idc);
    else
	pnw__H264_prepare_sequence_header(cmdbuf->header_mem_p + ctx->seq_header_ofs,
		pSequenceParams->picture_width_in_mbs,
		pSequenceParams->picture_height_in_mbs,IMG_TRUE,&VUI_Params,&sCrop,
		pSequenceParams->level_idc, ctx->profile_idc);

    pnw_cmdbuf_insert_command_package(ctx->obj_context, 
                                      ctx->ParallelCores - 1, /* Send to the last core as this will complete first */ 
                                      MTX_CMDID_DO_HEADER,
                                      &cmdbuf->header_mem,
                                      ctx->seq_header_ofs);
    free(pSequenceParams);

    return VA_STATUS_SUCCESS;
}

static VAStatus pnw__H264ES_process_picture_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int i;
    VAEncPictureParameterBufferH264 *pBuffer;
    pnw_cmdbuf_p cmdbuf = ctx->obj_context->pnw_cmdbuf;

    ASSERT(obj_buffer->type == VAEncPictureParameterBufferType);

    if ((obj_buffer->num_elements != 1) ||
        (obj_buffer->size != sizeof(VAEncPictureParameterBufferH264)))
    {
        return VA_STATUS_ERROR_UNKNOWN;
    }

    /* Transfer ownership of VAEncPictureParameterBufferH264 data */
    pBuffer = (VAEncPictureParameterBufferH264 *) obj_buffer->buffer_data;
    obj_buffer->buffer_data = NULL;
    obj_buffer->size = 0;

    ctx->ref_surface = SURFACE(pBuffer->reference_picture);
    ctx->dest_surface = SURFACE(pBuffer->reconstructed_picture);
    ctx->coded_buf = BUFFER(pBuffer->coded_buf);

    ASSERT(ctx->Width == pBuffer->picture_width);
    ASSERT(ctx->Height == pBuffer->picture_height);

    if (NULL == ctx->coded_buf)
    {
	psb__error_message("%s L%d Invalid coded buffer handle\n", __FUNCTION__, __LINE__); 
	free(pBuffer);	
	return VA_STATUS_ERROR_INVALID_BUFFER;
    }
    /* For H264, PicHeader only needed in the first picture*/
    if( !(ctx->obj_context->frame_count) ) {
        cmdbuf = ctx->obj_context->pnw_cmdbuf;
    
        pnw_cmdbuf_insert_command_package(ctx->obj_context, 
                                          ctx->ParallelCores - 1, /* Send to the last core as this will complete first */ 
                                          MTX_CMDID_DO_HEADER,
                                          &cmdbuf->header_mem,
                                          ctx->pic_header_ofs);

        pnw__H264_prepare_picture_header(cmdbuf->header_mem_p + ctx->pic_header_ofs, IMG_FALSE, ctx->sRCParams.QCPOffset);
    }

    if (ctx->ParallelCores == 1)
    {
	ctx->coded_buf_per_slice = 0;
	psb__information_message("TOPAZ: won't splite coded buffer(%d) since only one slice being encoded\n",
		ctx->coded_buf->size);
    }
    else
    {
	/*Make sure DMA start is 128bits alignment*/
	ctx->coded_buf_per_slice = (ctx->coded_buf->size / ctx->ParallelCores) & (~0xf) ;	
	psb__information_message("TOPAZ: the size of coded_buf per slice %d( Total %d) \n", ctx->coded_buf_per_slice,
		ctx->coded_buf->size);
    }

    /* Prepare START_PICTURE params */
    /* FIXME is really need multiple picParams? Need multiple calculate for each? */
    for(i = (ctx->ParallelCores - 1); i >= 0; i--)
        vaStatus = pnw_RenderPictureParameter(ctx, i);
    
    free(pBuffer);
    return vaStatus;
}

static VAStatus pnw__H264ES_process_slice_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    /* Prepare InParams for macros of current slice, insert slice header, insert do slice command */
    VAEncSliceParameterBuffer *pBuffer;
    pnw_cmdbuf_p cmdbuf = ctx->obj_context->pnw_cmdbuf;
    unsigned int MBSkipRun, FirstMBAddress;
    PIC_PARAMS *psPicParams = (PIC_PARAMS *)(cmdbuf->pic_params_p);
    int i;
    unsigned char is_intra = 0;

    ASSERT(obj_buffer->type == VAEncSliceParameterBufferType);

    cmdbuf = ctx->obj_context->pnw_cmdbuf;
    psPicParams = cmdbuf->pic_params_p;

    /* Transfer ownership of VAEncPictureParameterBufferH264 data */
    pBuffer = (VAEncSliceParameterBuffer *) obj_buffer->buffer_data;
    obj_buffer->size = 0;

    /* H264: Calculate number of Mskip to skip IF this was a skip frame */
    MBSkipRun = (ctx->Width * ctx->Height) / 256;

    /* 
    if (0 == pBuffer->start_row_number)
    {
	if (pBuffer->slice_flags.bits.is_intra) 
	    RELOC_PIC_PARAMS(&psPicParams->InParamsBase, ctx->in_params_ofs, cmdbuf->topaz_in_params_I);
	else
	    RELOC_PIC_PARAMS(&psPicParams->InParamsBase, ctx->in_params_ofs, cmdbuf->topaz_in_params_P);
    }
    */
    /*
    if (0 == pBuffer->start_row_number)
        RELOC_PIC_PARAMS_PNW(&psPicParams->InParamsBase, ctx->in_params_ofs, &cmdbuf->topaz_in_params);
    */


    if (getenv("PSB_VIDEO_DUAL_CORE") != NULL) 
    {
	    /*Need to replace unneccesary MTX_CMDID_STARTPICs with MTX_CMDID_PAD*/
	    for (i = 0; i < (ctx->ParallelCores - obj_buffer->num_elements); i++)
	    {
		    *(cmdbuf->cmd_idx_saved + i * 4) &= (~MTX_CMDWORD_ID_MASK);
		    *(cmdbuf->cmd_idx_saved + i * 4) |= MTX_CMDID_PAD;
	    }
	    if (ctx->ParallelCores > obj_buffer->num_elements)
	    {
		    psb__information_message(" Remove unneccesary %d MTX_CMDID_STARTPIC commands from cmdbuf\n",
				    ctx->ParallelCores - obj_buffer->num_elements);
		    ctx->ParallelCores = obj_buffer->num_elements;
	    }
    }

    for(i = 0; i < obj_buffer->num_elements; i++) {
	/*Todo list:
	 *1.Insert Do header command
	 *2.setup InRowParams
	 *3.setup Slice params
	 *4.Insert Do slice command
	 * */
	unsigned char deblock_idc;

	/*If the frame is skipped, it shouldn't be a I frame*/
	is_intra = (ctx->sRCParams.RCEnable && ctx->sRCParams.FrameSkip) ? 0 : pBuffer->slice_flags.bits.is_intra; 
	deblock_idc = pBuffer->slice_flags.bits.disable_deblocking_filter_idc;

	if((ctx->NumCores > 1) && (deblock_idc == 0))
	    deblock_idc = 2;

	FirstMBAddress = (pBuffer->start_row_number * ctx->Width) / 16; 

	memset(cmdbuf->header_mem_p + ctx->slice_header_ofs 
		+ ctx->obj_context->slice_count * HEADER_SIZE,
		0,
		HEADER_SIZE);

	/* Insert Do Header command, relocation is needed */
	pnw__H264_prepare_slice_header(cmdbuf->header_mem_p + ctx->slice_header_ofs 
	    + ctx->obj_context->slice_count * HEADER_SIZE,
		is_intra,
		pBuffer->slice_flags.bits.disable_deblocking_filter_idc,
		ctx->obj_context->frame_count,
		FirstMBAddress,
		MBSkipRun, 0);

	/* ensure that this slice is consequtive to that last processed by the target core */
	/*
	   ASSERT( -1 == ctx->LastSliceNum[ctx->SliceToCore]
	   || ctx->obj_context->slice_count == 1 + ctx->LastSliceNum[ctx->SliceToCore] );
	   */
	/* note the slice number the target core is now processing */
	ctx->LastSliceNum[ctx->SliceToCore] = ctx->obj_context->slice_count;

	pnw_cmdbuf_insert_command_package(ctx->obj_context,
		ctx->SliceToCore,
		MTX_CMDID_DO_HEADER,
		&cmdbuf->header_mem,
		ctx->slice_header_ofs + ctx->obj_context->slice_count * HEADER_SIZE);

	if ( 0 == ctx->SliceToCore )
	{
	    ctx->SliceToCore = ctx->ParallelCores;
	}
	ctx->SliceToCore--;

	if (!(ctx->sRCParams.RCEnable && ctx->sRCParams.FrameSkip))
	{
	    /*Only reset on the first frame. It's more effective than DDK. Have confirmed with IMG*/
	    if (ctx->obj_context->frame_count==0)
		pnw_reset_encoder_params(ctx);
	    if((pBuffer->start_row_number == 0) && pBuffer->slice_flags.bits.is_intra){ 
		ctx->BelowParamsBufIdx = (ctx->BelowParamsBufIdx + 1) & 0x1;
	    }

	    //	    if (VAEncSliceParameter_Equal(&ctx->slice_param_cache[(pBuffer->slice_flags.bits.is_intra? 0:1)], pBuffer)==0) {
	    ///* cache current param parameters */
	    //memcpy(&ctx->slice_param_cache[(pBuffer->slice_flags.bits.is_intra ? 0:1)],
	    //pBuffer, sizeof(VAEncSliceParameterBuffer));
	    //
	    ///* Setup InParams value*/
	    //pnw_setup_slice_params(ctx,
	    //pBuffer->start_row_number * 16,
	    //pBuffer->slice_height*16,
	    //pBuffer->slice_flags.bits.is_intra,
	    //ctx->obj_context->frame_count > 0,
	    //psPicParams->sInParams.SeInitQP);
	    //}FIXME Not use slice_param_cache temporary for simplify 

	    pnw_setup_slice_params(ctx,
		    pBuffer->start_row_number * 16,
		    pBuffer->slice_height*16,
		    pBuffer->slice_flags.bits.is_intra,
		    ctx->obj_context->frame_count > 0,
		    psPicParams->sInParams.SeInitQP);

	    /* Insert do slice command and setup related buffer value */ 
	    pnw__send_encode_slice_params(ctx,
		    pBuffer->slice_flags.bits.is_intra,
		    pBuffer->start_row_number * 16,
		    deblock_idc,
		    ctx->obj_context->frame_count,
		    pBuffer->slice_height*16,
		    ctx->obj_context->slice_count);

	    psb__information_message("Now frame_count/slice_count is %d/%d\n", 
		    ctx->obj_context->frame_count, ctx->obj_context->slice_count);
	}
	ctx->obj_context->slice_count++;
	pBuffer++; /* Move to the next buffer */

	ASSERT(ctx->obj_context->slice_count < MAX_SLICES_PER_PICTURE);
    }

    free(obj_buffer->buffer_data );
    obj_buffer->buffer_data = NULL;

    return VA_STATUS_SUCCESS;
}

static VAStatus pnw_H264ES_RenderPicture(
	object_context_p obj_context,
	object_buffer_p *buffers,
	int num_buffers)
{
    INIT_CONTEXT_H264ES;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int i;
        
    psb__information_message("pnw_H264ES_RenderPicture\n");

    for(i = 0; i < num_buffers; i++) {
        object_buffer_p obj_buffer = buffers[i];

        switch( obj_buffer->type) 
        {
        case VAEncSequenceParameterBufferType:
            psb__information_message("pnw_H264_RenderPicture got VAEncSequenceParameterBufferType\n");
            vaStatus = pnw__H264ES_process_sequence_param(ctx, obj_buffer);
            DEBUG_FAILURE;
            break;

        case VAEncPictureParameterBufferType:
            psb__information_message("pnw_H264_RenderPicture got VAEncPictureParameterBuffer\n");
            vaStatus = pnw__H264ES_process_picture_param(ctx, obj_buffer);
            DEBUG_FAILURE;
            break;

        case VAEncSliceParameterBufferType:
            psb__information_message("pnw_H264_RenderPicture got VAEncSliceParameterBufferType\n");
            vaStatus = pnw__H264ES_process_slice_param(ctx, obj_buffer);
            DEBUG_FAILURE;
            break;

        default:
            vaStatus = VA_STATUS_ERROR_UNKNOWN;
            DEBUG_FAILURE;
        }
        if (vaStatus != VA_STATUS_SUCCESS)
        {
            break;
        }
    }

    return vaStatus;
}

static VAStatus pnw_H264ES_EndPicture(
    object_context_p obj_context)
{
    INIT_CONTEXT_H264ES;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    psb__information_message("pnw_H264ES_EndPicture\n");

    vaStatus = pnw_EndPicture(ctx);

    return vaStatus;
}


struct format_vtable_s pnw_H264ES_vtable = {
  queryConfigAttributes: pnw_H264ES_QueryConfigAttributes,
  validateConfig: pnw_H264ES_ValidateConfig,
  createContext: pnw_H264ES_CreateContext,
  destroyContext: pnw_H264ES_DestroyContext,
  beginPicture: pnw_H264ES_BeginPicture,
  renderPicture: pnw_H264ES_RenderPicture,
  endPicture: pnw_H264ES_EndPicture
};
