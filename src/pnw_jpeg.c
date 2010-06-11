/*
 * Copyright (c) 2007 Intel Corporation. All Rights Reserved.
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
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "psb_def.h"
#include "psb_surface.h"
#include "psb_cmdbuf.h"
#include "pnw_jpeg.h"
#include "pnw_hostcode.h"
#include "pnw_hostheader.h"
#include "pnw_hostjpeg.h"

#define TOPAZ_MPEG4_MAX_BITRATE 16000000

#define INIT_CONTEXT_JPEG	context_ENC_p ctx = (context_ENC_p) obj_context->format_data
#define SURFACE(id)    ((object_surface_p) object_heap_lookup( &ctx->obj_context->driver_data->surface_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &ctx->obj_context->driver_data->buffer_heap, id ))



static void pnw_jpeg_QueryConfigAttributes(
    VAProfile profile,
    VAEntrypoint entrypoint,
    VAConfigAttrib *attrib_list,
    int num_attribs )
{
    int i;

    psb__information_message("pnw_jpeg_QueryConfigAttributes\n");

    /* RateControl attributes */
    for (i = 0; i < num_attribs; i++) {
        switch (attrib_list[i].type) {
        case VAConfigAttribRTFormat:
            break;
        default:
            attrib_list[i].value = VA_ATTRIB_NOT_SUPPORTED;
            break;
        }
    }
    
    return;
}


static VAStatus pnw_jpeg_ValidateConfig(
    object_config_p obj_config )
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    psb__information_message("pnw_jpeg_ValidateConfig\n");

    return vaStatus;

}

/*Init JPEG context. Ported from IMG_JPEG_EncoderInitialise*/
static VAStatus pnw_jpeg_CreateContext(
    object_context_p obj_context,
    object_config_p obj_config )
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_ENC_p ctx;
    TOPAZSC_JPEG_ENCODER_CONTEXT *jpeg_ctx_p;

    psb__information_message("pnw_jpeg_CreateContext\n");

    vaStatus = pnw_CreateContext(obj_context, obj_config, 1);
    if(VA_STATUS_SUCCESS != vaStatus) 
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    ctx = (context_ENC_p) obj_context->format_data;

    ctx->eCodec = IMG_CODEC_JPEG; 
    ctx->eFormat = IMG_CODEC_PL12;
//    ctx->eFormat = IMG_CODEC_IYUV;

    ctx->Slices = 2;
    ctx->ParallelCores = 2;
    ctx->NumCores = 2;
    ctx->jpeg_ctx = (TOPAZSC_JPEG_ENCODER_CONTEXT * )malloc(sizeof(TOPAZSC_JPEG_ENCODER_CONTEXT));

    if (NULL == ctx->jpeg_ctx)
	return VA_STATUS_ERROR_ALLOCATION_FAILED;

    jpeg_ctx_p = ctx->jpeg_ctx;
    jpeg_ctx_p->eFormat = ctx->eFormat;

    jpeg_ctx_p->sScan_Encode_Info.ui8NumberOfCodedBuffers = ctx->NumCores;
    
    jpeg_ctx_p->sScan_Encode_Info.aBufferTable = 
	(TOPAZSC_JPEG_BUFFER_INFO *)malloc(sizeof(TOPAZSC_JPEG_BUFFER_INFO) 
		    * jpeg_ctx_p->sScan_Encode_Info.ui8NumberOfCodedBuffers); 
   
    if (NULL == jpeg_ctx_p->sScan_Encode_Info.aBufferTable)
	return VA_STATUS_ERROR_ALLOCATION_FAILED;
    memset(jpeg_ctx_p->sScan_Encode_Info.aBufferTable, 0, 
	sizeof(TOPAZSC_JPEG_BUFFER_INFO) * jpeg_ctx_p->sScan_Encode_Info.ui8NumberOfCodedBuffers);

    jpeg_ctx_p->ui32OutputWidth = ctx->Width;
    jpeg_ctx_p->ui32OutputHeight = ctx->Height;
 
    jpeg_ctx_p->ui32SizePerCodedBuffer = PNW_JPEG_CODED_BUF_SIZE(ctx->Width, ctx->Height, ctx->NumCores);

    psb__information_message("SizePerCodedBuffer :%d\n", jpeg_ctx_p->ui32SizePerCodedBuffer);
    jpeg_ctx_p->ctx = ctx;
    /*Reuse header_mem(76*4 bytes) and pic_params_size(256 bytes)
     *  as pMemInfoMTXSetup(JPEG_MTX_DMA_SETUP 24x4 bytes) and 
     *  pMemInfoTableBlock JPEG_MTX_QUANT_TABLE(128byes)*/ 
    return vaStatus;
}


static void pnw_jpeg_DestroyContext(
    object_context_p obj_context)
{
    context_ENC_p ctx;

    psb__information_message("pnw_jpeg_DestroyPicture\n");

    ctx = (context_ENC_p)(obj_context->format_data);

    if (ctx->jpeg_ctx)
    {
	if (ctx->jpeg_ctx->sScan_Encode_Info.aBufferTable)
	{
	    free(ctx->jpeg_ctx->sScan_Encode_Info.aBufferTable);
	    ctx->jpeg_ctx->sScan_Encode_Info.aBufferTable = NULL;
	}

	free(ctx->jpeg_ctx);
    }
    pnw_DestroyContext(obj_context);

}

static VAStatus pnw_jpeg_BeginPicture(
    object_context_p obj_context)
{
    INIT_CONTEXT_JPEG;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int ret;
    pnw_cmdbuf_p cmdbuf;

    psb__information_message("pnw_jpeg_BeginPicture\n");

    ctx->src_surface = ctx->obj_context->current_render_target;

    /* Initialise the command buffer */
    ret = pnw_context_get_next_cmdbuf(ctx->obj_context);
    if(ret) {
        psb__information_message("get next cmdbuf fail\n"); 
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        return vaStatus;
    }
    cmdbuf = ctx->obj_context->pnw_cmdbuf;
   
    /* map start_pic param */
    vaStatus = psb_buffer_map(&cmdbuf->pic_params, &cmdbuf->pic_params_p);
    if(vaStatus) {
        return vaStatus;
    }
    vaStatus = psb_buffer_map(&cmdbuf->header_mem, &cmdbuf->header_mem_p);
    if (vaStatus) {
        psb_buffer_unmap(&cmdbuf->pic_params);
        return vaStatus;
    }
    /*vaStatus = psb_buffer_map(&cmdbuf->slice_params, &cmdbuf->slice_params_p);
    if (vaStatus) {
        psb_buffer_unmap(&cmdbuf->pic_params);
        psb_buffer_unmap(&cmdbuf->header_mem);
        return vaStatus;
    }*/

    /*Store the QMatrix data*/
    ctx->jpeg_ctx->pMemInfoTableBlock = cmdbuf->pic_params_p; 
    ctx->jpeg_ctx->psTablesBlock = (JPEG_MTX_QUANT_TABLE *)ctx->jpeg_ctx->pMemInfoTableBlock;

    /*Store MTX_SETUP data*/
    ctx->jpeg_ctx->pMemInfoMTXSetup = cmdbuf->header_mem_p;
    ctx->jpeg_ctx->pMTXSetup = (JPEG_MTX_DMA_SETUP*)ctx->jpeg_ctx->pMemInfoMTXSetup;

    ctx->jpeg_ctx->pMTXSetup->ui32ComponentsInScan=PNW_JPEG_COMPONENTS_NUM;
 
    if ( ctx->obj_context->frame_count==0) { /* first picture */

        psb_driver_data_p driver_data=ctx->obj_context->driver_data;

        *cmdbuf->cmd_idx++ = ((MTX_CMDID_SW_NEW_CODEC & MTX_CMDWORD_ID_MASK) << MTX_CMDWORD_ID_SHIFT) |
                             (((driver_data->drm_context & MTX_CMDWORD_COUNT_MASK) << MTX_CMDWORD_COUNT_SHIFT));
    	pnw_cmdbuf_insert_command_param(ctx->eCodec);
        pnw_cmdbuf_insert_command_param((ctx->Width<<16)|ctx->Height);
    }

    pnw_jpeg_set_default_qmatix(ctx->jpeg_ctx->pMemInfoTableBlock);
  
    InitializeJpegEncode(ctx->jpeg_ctx, ctx->src_surface);

    return vaStatus;
}

static VAStatus pnw__jpeg_process_picture_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    VAEncPictureParameterBufferJPEG *pBuffer;
    pnw_cmdbuf_p cmdbuf = ctx->obj_context->pnw_cmdbuf;
    BUFFER_HEADER *pBufHeader;
    //unsigned long *pPictureHeaderMem;
    //MTX_HEADER_PARAMS *psPicHeader;
    int i;
    TOPAZSC_JPEG_ENCODER_CONTEXT *jpeg_ctx = ctx->jpeg_ctx;
    IMG_ERRORCODE rc;

    ASSERT(obj_buffer->type == VAEncPictureParameterBufferType);

    if((obj_buffer->num_elements != 1) ||
       (obj_buffer->size != sizeof(VAEncPictureParameterBufferJPEG)))
    {
        return VA_STATUS_ERROR_UNKNOWN;
    }

    /* Transfer ownership of VAEncPictureParameterBufferMPEG4 data */
    pBuffer = (VAEncPictureParameterBufferJPEG *) obj_buffer->buffer_data;
    obj_buffer->buffer_data = NULL;
    obj_buffer->size = 0;

    ASSERT(ctx->Width == pBuffer->picture_width);
    ASSERT(ctx->Height == pBuffer->picture_height);

    ctx->coded_buf = BUFFER(pBuffer->coded_buf);

    free(pBuffer);

    if (NULL == ctx->coded_buf)
    {
	psb__error_message("%s L%d Invalid coded buffer handle\n", __FUNCTION__, __LINE__);
	return VA_STATUS_ERROR_INVALID_BUFFER;
    }

    psb__information_message("Set Quant Tables\n");
    /*Set Quant Tables*/
    for (i = ctx->NumCores - 1; i >= 0; i--)
	pnw_cmdbuf_insert_command_package(ctx->obj_context, 
		i, 
		MTX_CMDID_SETQUANT,
		&cmdbuf->pic_params,
		0);

    for (i = 0; i < 10 ; i++)
	psb__information_message("Quant Table %d: %d\n", i, *((unsigned char *)cmdbuf->pic_params_p + i));
    psb__information_message("Coded buffer total size is %d, per coded buffer size is %d\n",
		ctx->coded_buf->size, jpeg_ctx->ui32SizePerCodedBuffer );

    vaStatus = psb_buffer_map(ctx->coded_buf->psb_buffer, &jpeg_ctx->jpeg_coded_buf.pMemInfo); 
    if (vaStatus) 
    {
	psb__error_message("ERROR: Map coded_buf failed!");
        return vaStatus;
    }
    jpeg_ctx->jpeg_coded_buf.ui32Size = ctx->coded_buf->size;
    jpeg_ctx->jpeg_coded_buf.sLock = BUFFER_FREE;
    jpeg_ctx->jpeg_coded_buf.ui32BytesWritten = 0;

    psb__information_message("Setup JPEG Tables\n");
    rc=SetupJPEGTables(ctx->jpeg_ctx, &jpeg_ctx->jpeg_coded_buf,  ctx->src_surface);

    if (rc!=IMG_ERR_OK) 
	return VA_STATUS_ERROR_UNKNOWN;

    psb__information_message("Write JPEG Headers to coded buf\n");

    pBufHeader = (BUFFER_HEADER *)jpeg_ctx->jpeg_coded_buf.pMemInfo;
    pBufHeader->ui32BytesUsed = 0; /* Not include BUFFER_HEADER*/
    rc=PrepareHeader(jpeg_ctx, &jpeg_ctx->jpeg_coded_buf, sizeof(BUFFER_HEADER), IMG_TRUE);
    if (rc!=IMG_ERR_OK) 
	return VA_STATUS_ERROR_UNKNOWN;

    pBufHeader->ui32Reserved3 = PNW_JPEG_HEADER_MAX_SIZE;//Next coded buffer offset
    pBufHeader->ui32BytesUsed = jpeg_ctx->jpeg_coded_buf.ui32BytesWritten - sizeof(BUFFER_HEADER);

    psb__information_message("JPEG Buffer Header size: %d, File Header size :%d, next codef buffer offset: %d\n", 
	sizeof(BUFFER_HEADER), pBufHeader->ui32BytesUsed, pBufHeader->ui32Reserved3);
    return vaStatus;
}

static VAStatus pnw__jpeg_process_qmatrix_param(context_ENC_p ctx, object_buffer_p obj_buffer)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    VAQMatrixBufferJPEG *pBuffer;
    JPEG_MTX_QUANT_TABLE* pQMatrix = (JPEG_MTX_QUANT_TABLE *)
	(ctx->jpeg_ctx->pMemInfoTableBlock);

    ASSERT(obj_buffer->type == VAQMatrixBufferType);

    pBuffer = (VAQMatrixBufferJPEG *) obj_buffer->buffer_data;

    if (0 != pBuffer->load_lum_quantiser_matrix)
    {
	memcpy(pQMatrix->aui8LumaQuantParams, 
		pBuffer->lum_quantiser_matrix, 
		QUANT_TABLE_SIZE_BYTES);
    }

    if (0 != pBuffer->load_chroma_quantiser_matrix)
    {
	memcpy(pQMatrix->aui8LumaQuantParams,
		pBuffer->chroma_quantiser_matrix,
		QUANT_TABLE_SIZE_BYTES);
    }

    free(obj_buffer->buffer_data);
    obj_buffer->buffer_data = NULL;
    
    return vaStatus;
}


static VAStatus pnw_jpeg_RenderPicture(
    object_context_p obj_context,
    object_buffer_p *buffers,
    int num_buffers)
{
    INIT_CONTEXT_JPEG;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int i;
    
    psb__information_message("pnw_jpeg_RenderPicture\n");

    for (i=0; i<num_buffers; i++) {
	object_buffer_p obj_buffer = buffers[i];

	switch (obj_buffer->type) {
	    case VAQMatrixBufferType:
		psb__information_message("pnw_jpeg_RenderPicture got VAEncSliceParameterBufferType\n");
		vaStatus = pnw__jpeg_process_qmatrix_param(ctx, obj_buffer);
		DEBUG_FAILURE;
		break;
	    case VAEncPictureParameterBufferType:
		psb__information_message("pnw_jpeg_RenderPicture got VAEncPictureParameterBufferType\n");
		vaStatus = pnw__jpeg_process_picture_param(ctx, obj_buffer);
		DEBUG_FAILURE;
		break;
	    default:
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
		DEBUG_FAILURE;
	}
    }

    return vaStatus;
}

/* Add Restart interval termination (RSTm)to coded buf 1 ~ NumCores-1*/
static inline VAStatus pnw_OutputResetIntervalToCB(IMG_UINT8 *pui8Buf, IMG_UINT8 ui8_marker)
{
   if (NULL == pui8Buf)	
	   return VA_STATUS_ERROR_UNKNOWN;
   /*Refer to CCITT Rec. T.81 (1992 E), B.2.1*/
   /*RSTm: Restart marker conditional marker which is placed between 
    * entropy-coded segments only if restartis enabled. There are 8 unique 
    * restart markers (m = 0 - 7) which repeat in sequence from 0 to 7, starting with
    * zero for each scan, to provide a modulo 8 restart interval count*/
   *pui8Buf++ = 0xff;
   *pui8Buf = ( ui8_marker | 0xd0 );  	 
   return 0;
}

 
static VAStatus pnw_jpeg_EndPicture(
    object_context_p obj_context)
{
    INIT_CONTEXT_JPEG;
    IMG_UINT16 ui16BCnt;
    TOPAZSC_JPEG_ENCODER_CONTEXT *pContext = ctx->jpeg_ctx;
    IMG_UINT32 rc = 0;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    BUFFER_HEADER* pBufHeader;
    pnw_cmdbuf_p cmdbuf = (pnw_cmdbuf_p)ctx->obj_context->pnw_cmdbuf;
    STREAMTYPEW s_streamW;
    void *pCodedBufStart = NULL;

    psb__information_message("pnw_jpeg_EndPicture\n");

    for (ui16BCnt = 0; ui16BCnt < pContext->sScan_Encode_Info.ui8NumberOfCodedBuffers
	    && pContext->sScan_Encode_Info.ui16SScan >= 0; ui16BCnt++)
    {
	pContext->sScan_Encode_Info.aBufferTable[ui16BCnt].ui16ScanNumber = pContext->sScan_Encode_Info.ui16SScan--;
	/*FIXME: if ui16CScan(initial value of ui16SScan) is larger than Numcores, we shuold wait for MTX idle 
	 * before sending more MTX_CMDID_ISSUEBUFF commands. This case may happen if (Width / 16)*(Height / 16)
	 * is odd, e.x QGIF. Then we should use only one core to encode it*/
	pContext->sScan_Encode_Info.aBufferTable[ui16BCnt].i8MTXNumber = pContext->sScan_Encode_Info.aBufferTable[ui16BCnt].ui16ScanNumber; 
	rc=SubmitScanToMTX(pContext, ui16BCnt, pContext->sScan_Encode_Info.aBufferTable[ui16BCnt].i8MTXNumber);
	if (rc != IMG_ERR_OK)
	{
	    vaStatus = VA_STATUS_ERROR_UNKNOWN; 
	    DEBUG_FAILURE;
	    return vaStatus;
	}
    }

    psb_buffer_unmap(&cmdbuf->pic_params);
    cmdbuf->pic_params_p = NULL;
    psb_buffer_unmap(&cmdbuf->header_mem);
    cmdbuf->header_mem_p = NULL;
    /*psb_buffer_unmap(&cmdbuf->slice_params);
    cmdbuf->slice_params_p = NULL;*/
    psb_buffer_unmap(ctx->coded_buf->psb_buffer);	
    pContext->jpeg_coded_buf.pMemInfo = NULL;
    if (pnw_context_flush_cmdbuf(ctx->obj_context)) {
	vaStatus = VA_STATUS_ERROR_UNKNOWN;
	return vaStatus;
    }

 /*   vaStatus = psb_buffer_map(&cmdbuf->slice_params, &cmdbuf->slice_params_p);
    if ( 0 != vaStatus)
    {
	psb__error_message("ERROR: map slice params failed\n");
	return vaStatus;
    }


    for (ui16BCnt = 0; ui16BCnt < pContext->sScan_Encode_Info.ui8NumberOfCodedBuffers; ui16BCnt++ ){
	pContext->sScan_Encode_Info.aBufferTable[ui16BCnt].pMemInfo = cmdbuf->slice_params_p +
	   ui16BCnt * pContext->ui32SizePerCodedBuffer;
	pBuffHdr =  (BUFFER_HEADER*)pContext->sScan_Encode_Info.aBufferTable[ui16BCnt].pMemInfo;
	psb__information_message("Coded Buffer %d, used bytes %d\n",pBuffHdr->ui32BytesUsed);
	if (pContext->jpeg_coded_buf.ui32BytesWritten + pBuffHdr->ui32BytesUsed < pContext->jpeg_coded_buf.ui32Size )
	{
	    psb__error_message("ERROR: There is no enough space in codedbuf!\n");
	    psb_buffer_unmap(ctx->coded_buf->psb_buffer);
	    return VA_STATUS_ERROR_UNKNOWN;
	}
	memcpy((void *)(pContext->jpeg_coded_buf.pMemInfo + pContext->jpeg_coded_buf.ui32BytesWritten), 
		(void *)(pContext->sScan_Encode_Info.aBufferTable[ui16BCnt].pMemInfo + sizeof(BUFFER_HEADER)),
		pBuffHdr->ui32BytesUsed);
	pContext->jpeg_coded_buf.ui32BytesWritten += pBuffHdr->ui32BytesUsed;
    }	
    */ 
    vaStatus = psb_buffer_map(ctx->coded_buf->psb_buffer, &pContext->jpeg_coded_buf.pMemInfo); 
    if (vaStatus) 
    {
	psb__error_message("ERROR: Map coded_buf failed!");
        return vaStatus;
    }
    pCodedBufStart = pContext->jpeg_coded_buf.pMemInfo;
    pBufHeader = (BUFFER_HEADER *)pContext->jpeg_coded_buf.pMemInfo;
     
    psb__information_message("Number of Coded buffers %d, Per Coded Buffer size : %d\n",
	pContext->sScan_Encode_Info.ui8NumberOfCodedBuffers, pContext->ui32SizePerCodedBuffer);

    /*The first part of coded buffer contains JPEG headers*/
    pBufHeader->ui32Reserved3 = PNW_JPEG_HEADER_MAX_SIZE;

    pContext->jpeg_coded_buf.ui32BytesWritten = 0; 

    for (ui16BCnt = 0; ui16BCnt < pContext->sScan_Encode_Info.ui8NumberOfCodedBuffers; ui16BCnt++ ) {
	pBufHeader = (BUFFER_HEADER *)pCodedBufStart;
	pBufHeader->ui32Reserved3 = PNW_JPEG_HEADER_MAX_SIZE + pContext->ui32SizePerCodedBuffer * ui16BCnt ;
	psb__information_message("Coded Buffer Part %d, size %d, next part offset: %d\n", 
		ui16BCnt, pBufHeader->ui32BytesUsed, pBufHeader->ui32Reserved3); 
        if (ui16BCnt > 0 && pContext->sScan_Encode_Info.ui8NumberOfCodedBuffers > 1)
	{
	    pnw_OutputResetIntervalToCB(
		(IMG_UINT8 *)(pCodedBufStart + sizeof(BUFFER_HEADER) + pBufHeader->ui32BytesUsed),
		ui16BCnt - 1);
	    pBufHeader->ui32BytesUsed += 2;
	    psb__information_message("Append 2 bytes Reset Interval %d to Coded Buffer Part %d\n",
		ui16BCnt - 1, ui16BCnt);	
	}		
        
	pContext->jpeg_coded_buf.ui32BytesWritten += pBufHeader->ui32BytesUsed;
	pCodedBufStart = pContext->jpeg_coded_buf.pMemInfo + pBufHeader->ui32Reserved3;
    } 
    pBufHeader = (BUFFER_HEADER *)pCodedBufStart;
    pBufHeader->ui32Reserved3 = 0; /*Last Part of Coded Buffer*/
    pContext->jpeg_coded_buf.ui32BytesWritten += pBufHeader->ui32BytesUsed;

    psb__information_message("Coded Buffer Part %d, size %d, next part offset: %d\n", 
		    ui16BCnt, pBufHeader->ui32BytesUsed, pBufHeader->ui32Reserved3); 

    s_streamW.Buffer = pCodedBufStart ;
    s_streamW.Buffer += (sizeof(BUFFER_HEADER) + pBufHeader->ui32BytesUsed); 
    fPutBitsToBuffer(&s_streamW, 2, END_OF_IMAGE);
    pBufHeader->ui32BytesUsed += 2;
    pContext->jpeg_coded_buf.ui32BytesWritten += 2;
   
    psb__information_message("Add two bytes to last part of coded buffer, total: %d\n", pContext->jpeg_coded_buf.ui32BytesWritten);
    psb_buffer_unmap(ctx->coded_buf->psb_buffer);
    return VA_STATUS_SUCCESS;
}


struct format_vtable_s pnw_JPEG_vtable = {
  queryConfigAttributes: pnw_jpeg_QueryConfigAttributes,
  validateConfig: pnw_jpeg_ValidateConfig,
  createContext: pnw_jpeg_CreateContext,
  destroyContext: pnw_jpeg_DestroyContext,
  beginPicture: pnw_jpeg_BeginPicture,
  renderPicture: pnw_jpeg_RenderPicture,
  endPicture: pnw_jpeg_EndPicture
};
