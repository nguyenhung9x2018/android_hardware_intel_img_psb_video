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

 */

#include "pnw_jpegdec.h"
#include "psb_def.h"
#include "psb_surface.h"
#include "psb_cmdbuf.h"

#include "hwdefs/reg_io2.h"
#include "hwdefs/msvdx_offsets.h"
#include "hwdefs/msvdx_cmds_io2.h"
#include "hwdefs/msvdx_vec_reg_io2.h"
#include "hwdefs/msvdx_vec_jpeg_reg_io2.h"
#include "hwdefs/dxva_fw_ctrl.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define GET_SURFACE_INFO_is_defined(psb_surface) ((int) (psb_surface->extra_info[0]))
#define SET_SURFACE_INFO_is_defined(psb_surface, val) psb_surface->extra_info[0] = (uint32_t) val;
#define GET_SURFACE_INFO_picture_structure(psb_surface) (psb_surface->extra_info[1])
#define SET_SURFACE_INFO_picture_structure(psb_surface, val) psb_surface->extra_info[1] = val;
#define GET_SURFACE_INFO_picture_coding_type(psb_surface) ((int) (psb_surface->extra_info[2]))
#define SET_SURFACE_INFO_picture_coding_type(psb_surface, val) psb_surface->extra_info[2] = (uint32_t) val;
#define GET_SURFACE_INFO_colocated_index(psb_surface) ((int) (psb_surface->extra_info[3]))
#define SET_SURFACE_INFO_colocated_index(psb_surface, val) psb_surface->extra_info[3] = (uint32_t) val;

#define JPEG_MAX_SETS_HUFFMAN_TABLES 2
#define JPEG_MAX_QUANT_TABLES 4

#define TABLE_CLASS_DC  0
#define TABLE_CLASS_AC  1
#define TABLE_CLASS_NUM 2

#define FE_STATE_BUFFER_SIZE    4096

#define JPEG_PROFILE_BASELINE 0

typedef enum
{
	STRIDE_256 = 0x0,
	STRIDE_384,
	STRIDE_512,
	STRIDE_768,
	STRIDE_1024,
	STRIDE_1536,
	STRIDE_2048,
	STRIDE_3072,
	STRIDE_4096,
	STRIDE_6144,
	STRIDE_8192,
	STRIDE_12288,
	STRIDE_16384,
	STRIDE_24576,
	STRIDE_32768,
} JPEG_STRIDE;

/************************************/
/* VLC table defines and structures */

/* max number of bits allowed in a VLC code */
#define JPG_VLC_MAX_CODE_LEN        (16)

/* max num bits to decode in any one decode direct operation */
#define JPG_VLC_MAX_DIRECT_WIDTH    (6)

/*
******************************************************************************

 This structure defines the VLC code used for a partiular symbol

******************************************************************************/
typedef struct
{
    uint16_t code;    // VLC code with valid data in top-most bits
    uint8_t code_legth;   // VLC code length
    uint8_t symbol;    // Symbol

} VLCSymbolCodeJPEG; //JPG_VLC_sSymbolCode


/*
******************************************************************************

 This structure describes a set of VLC codes for a particular Huffman tree

******************************************************************************/
typedef struct
{
    uint32_t num_codes;
    uint32_t min_len;
    uint32_t max_len;

} VLCSymbolStatsJPEG; //JPG_VLC_sSymbolStats


/*
******************************************************************************

 This structure describes the generated VLC code table

******************************************************************************/
typedef struct
{
    uint32_t size;
    uint32_t initial_width;
    uint32_t initial_opcode;

} VLCTableStatsJPEG; //JPG_VLC_sTableStats

/************************************/
/************************************/

static const uint8_t inverse_zigzag[64] =
{
    0x00, 0x01, 0x05, 0x06, 0x0e, 0x0f, 0x1b, 0x1c,
	0x02, 0x04, 0x07, 0x0D, 0x10, 0x1a, 0x1d, 0x2a,
	0x03, 0x08, 0x0C, 0x11, 0x19, 0x1e, 0x29, 0x2b,
	0x09, 0x0B, 0x12, 0x18, 0x1f, 0x28, 0x2c, 0x35,
	0x0A, 0x13, 0x17, 0x20, 0x27, 0x2d, 0x34, 0x36,
	0x14, 0x16, 0x21, 0x26, 0x2e, 0x33, 0x37, 0x3c,
	0x15, 0x22, 0x25, 0x2f, 0x32, 0x38, 0x3b, 0x3d,
	0x23, 0x24, 0x30, 0x31, 0x39, 0x3a, 0x3e, 0x3f
};

/**************************************/
/* JPEG VLC Table defines and OpCodes */

#define JPG_MAKE_MASK(X) ((1<<(X))-1)

#define JPG_INSTR_OP_CODE_WIDTH (3)
#define JPG_INSTR_SHIFT_WIDTH   (3)
#define JPG_INSTR_OFFSET_WIDTH  (9)

#define JPG_INSTR_OP_CODE_MASK  JPG_MAKE_MASK(JPG_JPG_INSTR_OP_CODE_WIDTH)
#define JPG_INSTR_SHIFT_MASK    JPG_MAKE_MASK(JPG_INSTR_SHIFT_WIDTH)
#define JPG_INSTR_OFFSET_MASK   JPG_MAKE_MASK(JPG_INSTR_OFFSET_WIDTH)


#define JPG_MAKE_OFFSET(code,width,leading) \
(((code<<leading) & JPG_MAKE_MASK(JPG_VLC_MAX_CODE_LEN)) >> (JPG_VLC_MAX_CODE_LEN-width))

typedef enum
{
    JPG_OP_DECODE_DIRECT = 0,
    JPG_OP_DECODE_LEADING_1,
    JPG_OP_DECODE_LEADING_0,

    JPG_OP_CODE_INVALID,

    JPG_OP_VALID_SYMBOL,
    JPG_OP_VALID_RANGE_EVEN,
    JPG_OP_VALID_RANGE_ODD,
    JPG_OP_VALID_RANGE_EVEN_SET_FLAG,

} VLCOpCodeJPEG; //

/**************************************/
/**************************************/


struct context_JPEG_s {
    object_context_p obj_context; /* back reference */

    uint32_t profile;

    /* Picture parameters */
    VAPictureParameterBufferJPEG *pic_params;

    uint32_t display_picture_width;    /* in pixels */
    uint32_t display_picture_height;    /* in pixels */

    uint32_t coded_picture_width;    /* in pixels */
    uint32_t coded_picture_height;    /* in pixels */

    uint32_t MCU_width;        
    uint32_t MCU_height;        
    uint32_t size_mb;                /* in macroblocks */
   
    uint8_t max_scalingH;
    uint8_t max_scalingV;

    /* List of VASliceParameterBuffers */
    object_buffer_p *slice_param_list;
    int slice_param_list_size;
    int slice_param_list_idx;

    /* VLC packed data */
    struct psb_buffer_s vlc_packed_table;
    /* FE state buffer */
    struct psb_buffer_s FE_state_buffer;

    /* Split buffers */
    int split_buffer_pending;

    uint32_t *p_range_mapping_base0;
    uint32_t *p_range_mapping_base1;
    uint32_t *p_slice_params; /* pointer to ui32SliceParams in CMD_HEADER */
    uint32_t *slice_first_pic_last;
    uint32_t *alt_output_flags;

    uint32_t vlctable_buffer_size;
//    uint8_t* bit_stream;
    uint32_t qmatrix_data[JPEG_MAX_QUANT_TABLES][16];
//	IMG_UINT32				uiRendecQuantTables[JPEG_MAX_QUANT_TABLES][16];
    uint32_t operating_mode;
    // Huffman table information as parsed from the bitstream
    VLCSymbolCodeJPEG* symbol_codes[TABLE_CLASS_NUM][JPEG_MAX_SETS_HUFFMAN_TABLES];
    VLCSymbolStatsJPEG symbol_stats[TABLE_CLASS_NUM][JPEG_MAX_SETS_HUFFMAN_TABLES];

    // Huffman table information compiled for the hardware
    uint32_t huffman_table_space;
    uint16_t* huffman_table_RAM;
//	IMG_UINT32				uiHuffmanTableSpace;
//	IMG_UINT16*				puiHuffmanTableRAM;
    VLCTableStatsJPEG	table_stats[TABLE_CLASS_NUM][JPEG_MAX_SETS_HUFFMAN_TABLES];

};

typedef struct context_JPEG_s *context_JPEG_p;

#define INIT_CONTEXT_JPEG    context_JPEG_p ctx = (context_JPEG_p) obj_context->format_data;

#define SURFACE(id)    ((object_surface_p) object_heap_lookup( &ctx->obj_context->driver_data->surface_heap, id ))

static void pnw_JPEG_QueryConfigAttributes(
    VAProfile profile,
    VAEntrypoint entrypoint,
    VAConfigAttrib *attrib_list,
    int num_attribs)
{
    /* No JPEG specific attributes */
}

static VAStatus pnw_JPEG_ValidateConfig(
    object_config_p obj_config)
{
    int i;
    /* Check all attributes */
    for (i = 0; i < obj_config->attrib_count; i++) {
        switch (obj_config->attrib_list[i].type) {
        case VAConfigAttribRTFormat:
            /* Ignore */
            break;

        default:
            return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;
        }
    }

    return VA_STATUS_SUCCESS;
}

static VAStatus psb__JPEG_check_legal_picture(object_context_p obj_context, object_config_p obj_config)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    if (NULL == obj_context) {
        vaStatus = VA_STATUS_ERROR_INVALID_CONTEXT;
        DEBUG_FAILURE;
        return vaStatus;
    }

    if (NULL == obj_config) {
        vaStatus = VA_STATUS_ERROR_INVALID_CONFIG;
        DEBUG_FAILURE;
        return vaStatus;
    }

    switch (obj_config->profile) {
    case VAProfileJPEGBaseline:
        if ((obj_context->picture_width <= 0) || (obj_context->picture_height <= 0)) {
            vaStatus = VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED;
        }
        break;

    default:
        vaStatus = VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
        break;
    }

    return vaStatus;
}

static void pnw_JPEG_DestroyContext(object_context_p obj_context);

static VAStatus pnw_JPEG_CreateContext(
    object_context_p obj_context,
    object_config_p obj_config)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    context_JPEG_p ctx;
    /* Validate flag */
    /* Validate picture dimensions */
    vaStatus = psb__JPEG_check_legal_picture(obj_context, obj_config);
    if (VA_STATUS_SUCCESS != vaStatus) {
        DEBUG_FAILURE;
        return vaStatus;
    }

    ctx = (context_JPEG_p) calloc(1, sizeof(struct context_JPEG_s));
    if (NULL == ctx) {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE;
        return vaStatus;
    }
    obj_context->format_data = (void*) ctx;
    ctx->obj_context = obj_context;
    ctx->pic_params = NULL;

    ctx->slice_param_list_size = 8;
    ctx->slice_param_list = (object_buffer_p*) calloc(1, sizeof(object_buffer_p) * ctx->slice_param_list_size);
    ctx->slice_param_list_idx = 0;

    if (NULL == ctx->slice_param_list) {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE;
        free(ctx);
        return vaStatus;
    }

    switch (obj_config->profile) {
    case VAProfileJPEGBaseline:
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "JPEG_PROFILE_BASELINE\n");
        ctx->profile = JPEG_PROFILE_BASELINE;
        break;

    default:
        ASSERT(0 == 1);
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
    }

    if (vaStatus == VA_STATUS_SUCCESS) {
        vaStatus = psb_buffer_create(obj_context->driver_data,
                                     FE_STATE_BUFFER_SIZE,
                                     psb_bt_vpu_only,
                                     &ctx->FE_state_buffer);
        DEBUG_FAILURE;
    }

    // TODO
    ctx->vlctable_buffer_size = 1984 * 2;
    if (vaStatus == VA_STATUS_SUCCESS) {
        vaStatus = psb_buffer_create(obj_context->driver_data,
                                     ctx->vlctable_buffer_size,
                                     psb_bt_cpu_vpu,
                                     &ctx->vlc_packed_table);
        DEBUG_FAILURE;
    }

    if (vaStatus != VA_STATUS_SUCCESS) {
        pnw_JPEG_DestroyContext(obj_context);
    }

    return vaStatus;
}

static void pnw_JPEG_DestroyContext(
    object_context_p obj_context)
{
    INIT_CONTEXT_JPEG
    int i;

    psb_buffer_destroy(&ctx->vlc_packed_table);
    psb_buffer_destroy(&ctx->FE_state_buffer);

    if (ctx->pic_params) {
        free(ctx->pic_params);
        ctx->pic_params = NULL;
    }

    if (ctx->slice_param_list) {
        free(ctx->slice_param_list);
        ctx->slice_param_list = NULL;
    }

    free(obj_context->format_data);
    obj_context->format_data = NULL;
}

static void compile_huffman_tables()
{
//	CDeviceMemAlloc* pDevMemAlloc;

//	pVlcPackedTableData->Lock( 0,(void**)&puiHuffmanTableRAM, &pDevMemAlloc );
	huffman_table_space = 1984;

       if (0 ==  psb_buffer_map(&ctx->vlc_packed_table, &huffman_table_RAM)) {

	// Compile Tables
	for( uint32_t table_class = 0; table_class < TABLE_CLASS_NUM; table_class++ )
	{
		for( uint32_t table_id = 0; table_id < JPEG_MAX_SETS_HUFFMAN_TABLES; table_id++ )
		{
			if( symbol_stats[table_class][table_id].num_codes )
			{
				JPG_VLC_CompileTable(	symbol_codes[table_class][table_id],
										&symbol_stats[table_class][table_id],
										huffman_table_space,
										huffman_table_RAM,
										&table_stats[table_class][table_id]	);

				huffman_table_space -= table_stats[table_class][table_id].size;
				huffman_table_RAM += table_stats[table_class][table_id].size;
			}
		}
	}
	huffman_table_space = 1984;
        psb_buffer_unmap(&ctx->vlc_packed_table);
    else {
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            DEBUG_FAILURE;
        }
}
}

static uint16_t jpg_vlc_valid_symbol(
    const VLCSymbolCodeJPEG * symbol_code, //psSymbolCode
    const uint32_t leading
)
{
    uint16_t entry = 0;

    IMG_ASSERT( (symbol_code->code_legth - leading - 1) >= 0 );

    /* VLC microcode entry for a valid symbol */
    entry |= (JPG_OP_VALID_SYMBOL << (JPG_INSTR_SHIFT_WIDTH+JPG_INSTR_OFFSET_WIDTH));
    entry |= ((symbol_code->code_legth - leading - 1) << JPG_INSTR_OFFSET_WIDTH);
    entry |= symbol_code->symbol;

    return entry;
}

static uint32_t
jpg_vlc_write_direct_command( 
    uint16_t * const vlc_ram, 
    uint32_t result_offset
)
{
    uint32_t width = 0x7fff & *vlc_ram;
    uint16_t entry = 0;

    /* check that the max width read from the VLC entry is valid */
    IMG_ASSERT( 0x8000 & *vlc_ram );

    /* limit to the maximum width for this algorithm */
    width = (width > JPG_VLC_MAX_DIRECT_WIDTH)? JPG_VLC_MAX_DIRECT_WIDTH: width;

    /* VLC microcode for decode direct command */
    entry |= (JPG_OP_DECODE_DIRECT << (JPG_INSTR_SHIFT_WIDTH+JPG_INSTR_OFFSET_WIDTH));
    entry |= ((width - 1) << JPG_INSTR_OFFSET_WIDTH);
    entry |= result_offset;

    /* write command */
    *vlc_ram = entry;

    return width;
}

static uint32_t
jpg_vlc_get_offset(
    const VLCSymbolCodeJPEG * symbol_code,
    uint32_t width,
    uint32_t leading
)
{
    uint32_t offset;

    /* lose bits already decoded */
    offset = symbol_code->code << leading;
    offset &= JPG_MAKE_MASK(JPG_VLC_MAX_CODE_LEN);
    /* convert truncated code to offset */
    offset >>= (JPG_VLC_MAX_CODE_LEN - width);

    return offset;
}

static uint32_t
jpg_vlc_decode_direct_symbols(
    const VLCSymbolCodeJPEG * symbol_code,
    uint16_t * const table_ram,
    const uint32_t width,
    const uint32_t leading
)
{
    uint32_t offset, limit;
    uint16_t entry;

    /* this function is only for codes short enough to produce valid symbols */
    IMG_ASSERT( symbol_code->code_legth <= leading + width );
    IMG_ASSERT( symbol_code->code_legth > leading );

    /* lose bits already decoded */
    offset = symbol_code->code << leading;
    offset &= JPG_MAKE_MASK(JPG_VLC_MAX_CODE_LEN);

    /* convert truncated code to offset */
    offset >>= (JPG_VLC_MAX_CODE_LEN - width);
    /* expand offset to encorporate undefined code bits */
    limit = offset + (1 << (width - (symbol_code->code_legth - leading)));

    /* for all code variants - insert symbol into the decode direct result table */
    entry = jpg_vlc_valid_symbol( symbol_code, leading );
    for ( ; offset < limit; offset++ )
    {
        table_ram[offset] = entry;
    }

    /* return the number of entries written */
    return limit - offset - 1;
}

static uint32_t
jpg_vlc_decode_direct(
    const VLCSymbolCodeJPEG * symbol_codes,
    const uint32_t num_codes,
    uint16_t * const table_ram,
    const uint32_t direct_width,
    const uint32_t leading_width,
    const uint32_t leading_pattern
)
{
    const uint32_t next_leading_width = leading_width + direct_width;
    const uint32_t next_leading_mask = JPG_MAKE_MASK(next_leading_width) << (JPG_VLC_MAX_CODE_LEN-next_leading_width);
    const uint32_t leading_mask = JPG_MAKE_MASK(leading_width) << (JPG_VLC_MAX_CODE_LEN-leading_width);

    uint32_t num_vlc_ops = 1 << direct_width;
    uint32_t next_section, next_width, next_leading_pattern;
    uint32_t offset;
    uint32_t i;

    /* sanity - check this decode direct will not exceed the max code len */
    IMG_ASSERT( next_leading_width <= JPG_VLC_MAX_CODE_LEN );

    /* set all VLC ops for this decode direct to invalid */
    for ( i = 0; i < num_vlc_ops; i++ )
    {
        table_ram[i] = (JPG_OP_CODE_INVALID << (JPG_INSTR_SHIFT_WIDTH+JPG_INSTR_OFFSET_WIDTH));
    }
 
    /* iterate over code table and insert VLC ops until */
    /* codes become too long for this iteration or we run out of codes */
    for ( i = 0; symbol_codes[i].code_legth <= next_leading_width && i < num_codes; i++ )
    {
        /* only use codes that match the specified leading portion */
        if ( (((uint32_t)symbol_codes[i].code) & leading_mask) == leading_pattern )
        {
            jpg_vlc_decode_direct_symbols( 
                &symbol_codes[i], 
                table_ram, 
                direct_width, 
                leading_width );
        } 
    }
    next_section = i;

    /* assign the longest code length for each remaining entry */
    for ( i = next_section; i < num_codes; i++ )
    {
        /* only use codes that match the specified leading portion */
        if ( (((uint32_t)symbol_codes[i].code) & leading_mask) == leading_pattern )
        {
            /* enable the unused VLC bit to indicate this is not a command */
            offset = jpg_vlc_get_offset(
                &symbol_codes[i], 
                direct_width, 
                leading_width );
            table_ram[offset] = 0x8000 | (symbol_codes[i].code_legth - next_leading_width);
        }     
    }

    /* for the remaining (long) codes */
    for ( i = next_section; i < num_codes; i++ )
    {
        /* only use codes that match the specified leading portion */
        if ( (((uint32_t)symbol_codes[i].code) & leading_mask) == leading_pattern )
        {
            /* if a command has not been written for this direct offset */
            offset = jpg_vlc_get_offset(
                &symbol_codes[i], 
                direct_width, 
                leading_width );

            if ( table_ram[offset] & 0x8000 )
            {
                /* write command the decode direct command */
                next_width = jpg_vlc_write_direct_command( 
                    &table_ram[offset], 
                    num_vlc_ops - offset );

                next_leading_pattern = (uint32_t)symbol_codes[i].code & next_leading_mask;

                /* decode direct recursive call */
                num_vlc_ops += jpg_vlc_decode_direct(
                    &symbol_codes[i],
                    num_codes - i,
                    &table_ram[num_vlc_ops],
                    next_width,
                    next_leading_width,
                    next_leading_pattern );
            }
        }     
    }

    return num_vlc_ops;
}

void JPG_VLC_CompileTable(
    const VLCSymbolCodeJPEG * symbol_codes,
    const VLCSymbolStatsJPEG * psSymbolStats,
    const uint32_t ram_size,
    uint16_t * table_ram,
    VLCTableStatsJPEG * ptable_stats
)
{
    ptable_stats->initial_width = 5;
    ptable_stats->initial_opcode = JPG_OP_DECODE_DIRECT;
    
    ptable_stats->size = jpg_vlc_decode_direct(
        symbol_codes,
        psSymbolStats->num_codes,
        table_ram,
        JPG_VLC_MAX_DIRECT_WIDTH,
        0,
        0 );

    IMG_ASSERT( ptable_stats->size <= ram_size );
}


#ifndef DE3_FIRMWARE
static void psb__JPEG_FE_state(context_JPEG_p ctx)
{
    psb_cmdbuf_p cmdbuf = ctx->obj_context->cmdbuf;

    /* See RENDER_BUFFER_HEADER */
    *cmdbuf->cmd_idx++ = CMD_HEADER_VC1;

    ctx->p_range_mapping_base0 = cmdbuf->cmd_idx++;
    ctx->p_range_mapping_base1 = cmdbuf->cmd_idx++;

    *ctx->p_range_mapping_base0 = 0;
    *ctx->p_range_mapping_base1 = 0;

    ctx->p_slice_params = cmdbuf->cmd_idx;
    *cmdbuf->cmd_idx++ = 0; /* ui32SliceParams */

    *cmdbuf->cmd_idx++ = 0; /* skip two lldma addr field */

    *cmdbuf->cmd_idx++  = 0;
    ctx->slice_first_pic_last = cmdbuf->cmd_idx++;
}
#else
static void psb__JPEG_FE_state(context_JPEG_p ctx)
{
    psb_cmdbuf_p cmdbuf = ctx->obj_context->cmdbuf;
    CTRL_ALLOC_HEADER *cmd_header = (CTRL_ALLOC_HEADER *)psb_cmdbuf_alloc_space(cmdbuf, sizeof(CTRL_ALLOC_HEADER));

    memset(cmd_header, 0, sizeof(CTRL_ALLOC_HEADER));
    cmd_header->ui32Cmd_AdditionalParams = CMD_CTRL_ALLOC_HEADER;
    //RELOC(cmd_header->ui32ExternStateBuffAddr, 0, &ctx->preload_buffer);
    cmd_header->ui32ExternStateBuffAddr = 0;
    cmd_header->ui32MacroblockParamAddr = 0; /* Only EC needs to set this */

    ctx->p_slice_params = &cmd_header->ui32SliceParams;
    cmd_header->ui32SliceParams = 0;

    ctx->slice_first_pic_last = &cmd_header->uiSliceFirstMbYX_uiPicLastMbYX;

    ctx->p_range_mapping_base0 = &cmd_header->ui32AltOutputAddr[0];
    ctx->p_range_mapping_base1 = &cmd_header->ui32AltOutputAddr[1];

    ctx->alt_output_flags = &cmd_header->ui32AltOutputFlags;
    cmd_header->ui32AltOutputFlags = 0;
}
#endif

static VAStatus psb__JPEG_process_picture_param(context_JPEG_p ctx, object_buffer_p obj_buffer)
{
    VAStatus vaStatus;
    ASSERT(obj_buffer->type == VAPictureParameterBufferType);
    ASSERT(obj_buffer->num_elements == 1);
    ASSERT(obj_buffer->size == sizeof(VAPictureParameterBufferJPEG));

    if ((obj_buffer->num_elements != 1) ||
        (obj_buffer->size != sizeof(VAPictureParameterBufferJPEG))) {
        return VA_STATUS_ERROR_UNKNOWN;
    }

    /* Transfer ownership of VAPictureParameterBufferJPEG data */
    if (ctx->pic_params) {
        free(ctx->pic_params);
    }
    ctx->pic_params = (VAPictureParameterBufferJPEG *) obj_buffer->buffer_data;
    ctx->display_picture_width = ctx->pic_params->image_width;
    ctx->display_picture_height = ctx->pic_params->image_height;

    ctx->coded_picture_width = ( ctx->display_picture_width + 7 ) & ( ~7 );
    ctx->coded_picture_height = ( ctx->display_picture_height + 7 ) & ( ~7 );
    ctx->max_scalingH = 0;
    ctx->max_scalingV = 0;
    for( uint8_t component_id = 0; component_id < num_components; component_id )
    {
        if (ctx->max_scalingH < ctx->pic_params->components->h_sampling_factor)
            ctx->max_scalingH = ctx->pic_params->components->h_sampling_factor;
        if (ctx->max_scalingV < ctx->pic_params->components->v_sampling_factor)
            ctx->max_scalingV = ctx->pic_params->components->v_sampling_factor;
    }

    ctx->MCU_width = ( ctx->coded_picture_width + ( 8 * ctx->max_scalingH ) - 1 ) / ( 8 * ctx->max_scalingH );
    ctx->MCU_height = ( ctx->coded_picture_height + ( 8 * ctx->max_scalingV ) - 1 ) / ( 8 * ctx->max_scalingV );

    obj_buffer->buffer_data = NULL;
    obj_buffer->size = 0;


    return VA_STATUS_SUCCESS;
}

static VAStatus psb__JPEG_add_slice_param(context_JPEG_p ctx, object_buffer_p obj_buffer)
{
    ASSERT(obj_buffer->type == VASliceParameterBufferType);
    if (ctx->slice_param_list_idx >= ctx->slice_param_list_size) {
        unsigned char *new_list;
        ctx->slice_param_list_size += 8;
        new_list = realloc(ctx->slice_param_list,
                           sizeof(object_buffer_p) * ctx->slice_param_list_size);
        if (NULL == new_list) {
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }
        ctx->slice_param_list = (object_buffer_p*) new_list;
    }
    ctx->slice_param_list[ctx->slice_param_list_idx] = obj_buffer;
    ctx->slice_param_list_idx++;
    return VA_STATUS_SUCCESS;
}

static void psb__JPEG_write_qmatrices(context_JPEG_p ctx)
{
    psb_cmdbuf_p cmdbuf = ctx->obj_context->cmdbuf;
    int i;

    psb_cmdbuf_rendec_start(cmdbuf, REG_MSVDX_VEC_IQRAM_OFFSET);

        for (i = 0; i < 16; i++) {
            psb_cmdbuf_rendec_write(cmdbuf, ctx->qmatrix_data[0][i]);
        }
        for (i = 0; i < 16; i++) {
            psb_cmdbuf_rendec_write(cmdbuf, ctx->qmatrix_data[1][i]);
        }
        for (i = 0; i < 16; i++) {
            psb_cmdbuf_rendec_write(cmdbuf, ctx->qmatrix_data[2][i]);
        }
        for (i = 0; i < 16; i++) {
            psb_cmdbuf_rendec_write(cmdbuf, ctx->qmatrix_data[3][i]);
        }    
   
    psb_cmdbuf_rendec_end(cmdbuf);
    /* psb_cmdbuf_rendec_end_block( cmdbuf ); */
}

static VAStatus psb__JPEG_process_iq_matrix(context_JPEG_p ctx, object_buffer_p obj_buffer)
{
    ctx->qmatrix_data = (VAIQMatrixBufferJPEG *) obj_buffer->buffer_data;
    ASSERT(obj_buffer->type == VAIQMatrixBufferType);
    ASSERT(obj_buffer->num_elements == 1);
    ASSERT(obj_buffer->size == sizeof(VAIQMatrixBufferJPEG));

    psb__JPEG_write_qmatrices(ctx);

    return VA_STATUS_SUCCESS;
}


static void psb__JPEG_write_huffman_tables(context_JPEG_p ctx)
{
    uint32_t reg_value;
    psb_cmdbuf_p cmdbuf = ctx->obj_context->cmdbuf;

    /* VLC Table */
    /* Write a LLDMA Cmd to transfer VLD Table data */
#ifndef DE3_FIRMWARE
    psb_cmdbuf_lldma_write_cmdbuf(cmdbuf, &ctx->vlc_packed_table, 0,
                                  ctx->vlctable_buffer_size,
                                  0, LLDMA_TYPE_VLC_TABLE);
#else
    psb_cmdbuf_dma_write_cmdbuf(cmdbuf, &ctx->vlc_packed_table, 0,
                                  ctx->vlctable_buffer_size, 0,
                                  DMA_TYPE_VLC_TABLE);
#endif

    // Write Table addresses
    psb_cmdbuf_reg_start_block(cmdbuf, 0);
    reg_value = 0;
    uint32_t table_address = 0;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_ADDR0, VLC_TABLE_ADDR0, table_address );
    table_address += table_stats[0][0].size;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_ADDR0, VLC_TABLE_ADDR1, table_address );
    psb_cmdbuf_reg_set(cmdbuf, REGISTER_OFFSET( MSVDX_VEC, CR_VEC_VLC_TABLE_ADDR0 ), reg_value);

    reg_value = 0;
    table_address += table_stats[0][1].size;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_ADDR2, VLC_TABLE_ADDR4, table_address );
    table_address += table_stats[1][0].size;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_ADDR2, VLC_TABLE_ADDR5, table_address );
    psb_cmdbuf_reg_set(cmdbuf, REGISTER_OFFSET( MSVDX_VEC, CR_VEC_VLC_TABLE_ADDR2 ), reg_value);
    
    psb_cmdbuf_reg_end_block(cmdbuf);

    // Write Initial Widths
    psb_cmdbuf_reg_start_block(cmdbuf, 0);
    reg_value = 0;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_INITIAL_WIDTH0, VLC_TABLE_INITIAL_WIDTH0, table_stats[0][0].initial_width );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_INITIAL_WIDTH0, VLC_TABLE_INITIAL_WIDTH1, table_stats[0][1].initial_width );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_INITIAL_WIDTH0, VLC_TABLE_INITIAL_WIDTH4, table_stats[1][0].initial_width );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_INITIAL_WIDTH0, VLC_TABLE_INITIAL_WIDTH5, table_stats[1][1].initial_width );
    psb_cmdbuf_reg_set(cmdbuf, REGISTER_OFFSET( MSVDX_VEC, CR_VEC_VLC_TABLE_INITIAL_WIDTH0 ), reg_value);
    
    // Write Initial Opcodes
    psb_cmdbuf_reg_start_block(cmdbuf, 0);
    reg_value = 0;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_INITIAL_OPCODE0, VLC_TABLE_INITIAL_OPCODE0, table_stats[0][0].initial_opcode );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_INITIAL_OPCODE0, VLC_TABLE_INITIAL_OPCODE1, table_stats[0][1].initial_opcode );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_INITIAL_OPCODE0, VLC_TABLE_INITIAL_OPCODE4, table_stats[1][0].initial_opcode );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_VLC_TABLE_INITIAL_OPCODE0, VLC_TABLE_INITIAL_OPCODE5, table_stats[1][1].initial_opcode );
    psb_cmdbuf_reg_set(cmdbuf, REGISTER_OFFSET( MSVDX_VEC, CR_VEC_VLC_TABLE_INITIAL_OPCODE0 ), reg_value);

    psb_cmdbuf_reg_end_block(cmdbuf);
    
}

static VAStatus psb__JPEG_process_huffman_table(context_JPEG_p ctx, object_buffer_p obj_buffer)
{
    VAHuffmanTableBufferJPEG *iq_matrix = (VAHuffmanTableBufferJPEG *) obj_buffer->buffer_data;
//    ASSERT(obj_buffer->type == VAHuffmanTableBufferType);
    ASSERT(obj_buffer->num_elements == 1);
    ASSERT(obj_buffer->size == sizeof(VAHuffmanTableBufferJPEG));

    compile_huffman_tables();
    psb__JPEG_write_huffman_tables(ctx);

    return VA_STATUS_SUCCESS;
}


static void psb__JPEG_set_operating_mode(context_JPEG_p ctx)
{
    psb_cmdbuf_p cmdbuf = ctx->obj_context->cmdbuf;
    operating_mode = 0;
    REGIO_WRITE_FIELD_LITE(operating_mode, MSVDX_CMDS, OPERATING_MODE, USE_EXT_ROW_STRIDE, 1 );
    REGIO_WRITE_FIELD_LITE( operating_mode, MSVDX_CMDS, OPERATING_MODE, ASYNC_MODE, 1 );
    REGIO_WRITE_FIELD_LITE( operating_mode, MSVDX_CMDS, OPERATING_MODE, CHROMA_FORMAT,	1);

    psb_cmdbuf_rendec_start(cmdbuf, RENDEC_REGISTER_OFFSET( MSVDX_CMDS, OPERATING_MODE ));

    psb_cmdbuf_rendec_write(cmdbuf, ctx->obj_context->operating_mode);

    psb_cmdbuf_rendec_end(cmdbuf);

}

static void psb__JPEG_set_reference_pictures(context_JPEG_p ctx)
{
    psb_cmdbuf_p cmdbuf = ctx->obj_context->cmdbuf;
    psb_surface_p target_surface = ctx->obj_context->current_render_target->psb_surface;

    psb_cmdbuf_rendec_start(cmdbuf, RENDEC_REGISTER_OFFSET(MSVDX_CMDS, REFERENCE_PICTURE_BASE_ADDRESSES));
    psb_cmdbuf_rendec_write(cmdbuf, 0);
    psb_cmdbuf_rendec_write(cmdbuf, 0);
    psb_cmdbuf_rendec_write(cmdbuf, 0);
    psb_cmdbuf_rendec_write(cmdbuf, 0);

    psb_cmdbuf_rendec_end(cmdbuf);

}

static void psb__JPEG_set_ent_dec(context_JPEG_p ctx)
{
    psb_cmdbuf_p cmdbuf = ctx->obj_context->cmdbuf;
    uint32_t reg_value;

    psb_cmdbuf_reg_start_block(cmdbuf, 0);
    reg_value = 0;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC, CR_VEC_ENTDEC_FE_CONTROL, ENTDEC_FE_MODE, 0 );
    psb_cmdbuf_reg_set(cmdbuf, REGISTER_OFFSET( MSVDX_VEC, CR_VEC_ENTDEC_FE_CONTROL ), reg_value);
    psb_cmdbuf_reg_end_block(cmdbuf);

    psb_cmdbuf_rendec_start(cmdbuf, RENDEC_REGISTER_OFFSET( MSVDX_VEC, CR_VEC_ENTDEC_BE_CONTROL ));
    reg_value = 0;
    REGIO_WRITE_FIELD( reg_value, MSVDX_VEC, CR_VEC_ENTDEC_BE_CONTROL, ENTDEC_BE_MODE, 0 );
    psb_cmdbuf_rendec_write(cmdbuf, reg_value);
    psb_cmdbuf_rendec_end(cmdbuf);

}

static void psb__JPEG_set_register(context_JPEG_p ctx, VASliceParameterBufferJPEG *slice_param)
{
    psb_cmdbuf_p cmdbuf = ctx->obj_context->cmdbuf;
    uint32_t reg_value;
    const uint32_t num_MCUs = ctx->MCU_width * ctx->MCU_height;
    const uint32_t num_MCUs_dec = slice_param->restart_interval ? min( slice_param->restart_interval , num_MCUs ) : num_MCUs;

    psb_cmdbuf_reg_start_block(cmdbuf, 0);
// CR_VEC_JPEG_FE_COMPONENTS
    reg_value = 0;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_COMPONENTS, MAX_V, ctx->max_scalingV);
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_COMPONENTS, MAX_H, ctx->max_scalingH );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_COMPONENTS, FE_COMPONENTS, slice_param->num_components );
    psb_cmdbuf_reg_set(cmdbuf, REGISTER_OFFSET( MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_COMPONENTS ), reg_value);

// CR_VEC_JPEG_FE_HEIGHT
    reg_value = 0;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_HEIGHT, FE_HEIGHT_MINUS1, ctx->coded_picture_height - 1 );
    psb_cmdbuf_reg_set(cmdbuf, REGISTER_OFFSET( MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_HEIGHT ), reg_value);

// CR_VEC_JPEG_FE_RESTART_POS
    reg_value = 0;
    psb_cmdbuf_reg_set(cmdbuf, REGISTER_OFFSET( MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_RESTART_POS ), reg_value);

// CR_VEC_JPEG_FE_WIDTH
    reg_value = 0;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_WIDTH, FE_WIDTH_MINUS1, ctx->coded_picture_width - 1 );
    psb_cmdbuf_reg_set(cmdbuf, REGISTER_OFFSET( MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_WIDTH ), reg_value);

// CR_VEC_JPEG_FE_ENTROPY_CODING
    reg_value = 0;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_ENTROPY_CODING, NUM_MCUS_LESS1, num_MCUs_dec - 1 );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_ENTROPY_CODING, TA3, psScanCtrl->componentList[3].uiHuffmanTableAC );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_ENTROPY_CODING, TD3, psScanCtrl->componentList[3].uiHuffmanTableDC );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_ENTROPY_CODING, TA2, psScanCtrl->componentList[2].uiHuffmanTableAC );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_ENTROPY_CODING, TD2, psScanCtrl->componentList[2].uiHuffmanTableDC );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_ENTROPY_CODING, TA1, psScanCtrl->componentList[1].uiHuffmanTableAC );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_ENTROPY_CODING, TD1, psScanCtrl->componentList[1].uiHuffmanTableDC );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_ENTROPY_CODING, TA0, psScanCtrl->componentList[0].uiHuffmanTableAC );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_ENTROPY_CODING, TD0, psScanCtrl->componentList[0].uiHuffmanTableDC );
    psb_cmdbuf_reg_set(cmdbuf, REGISTER_OFFSET( MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_ENTROPY_CODING ), reg_value);

// CR_VEC_JPEG_FE_SCALING
    reg_value = 0;
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_SCALING, FE_V3, psScanCtrl->componentList[3].uiScalingV );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_SCALING, FE_H3, psScanCtrl->componentList[3].uiScalingH );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_SCALING, FE_V2, psScanCtrl->componentList[2].uiScalingV );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_SCALING, FE_H2, psScanCtrl->componentList[2].uiScalingH );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_SCALING, FE_V1, psScanCtrl->componentList[1].uiScalingV );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_SCALING, FE_H1, psScanCtrl->componentList[1].uiScalingH );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_SCALING, FE_V0, psScanCtrl->componentList[0].uiScalingV );
    REGIO_WRITE_FIELD_LITE( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_SCALING, FE_H0, psScanCtrl->componentList[0].uiScalingH );
    psb_cmdbuf_reg_set(cmdbuf, REGISTER_OFFSET( MSVDX_VEC_JPEG, CR_VEC_JPEG_FE_SCALING ), reg_value);
    psb_cmdbuf_reg_end_block(cmdbuf);

    psb_cmdbuf_rendec_start(cmdbuf, RENDEC_REGISTER_OFFSET( MSVDX_VEC_JPEG, CR_VEC_JPEG_BE_HEIGHT ));
// CR_VEC_JPEG_BE_HEIGHT
    reg_value = 0;
    REGIO_WRITE_FIELD( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_BE_HEIGHT, BE_HEIGHT_MINUS1, picParams.wCodedHeight - 1 );
    psb_cmdbuf_rendec_write(cmdbuf, reg_value);

// CR_VEC_JPEG_BE_WIDTH
    reg_value = 0;
    REGIO_WRITE_FIELD( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_BE_WIDTH, BE_WIDTH_MINUS1, ctx->coded_picture_width - 1 );
    psb_cmdbuf_rendec_write(cmdbuf, reg_value);

// CR_VEC_JPEG_BE_QUANTISATION
    reg_value = 0;
    REGIO_WRITE_FIELD( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_BE_QUANTISATION, TQ3, psScanCtrl->componentList[3].uiQuantTable );
    REGIO_WRITE_FIELD( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_BE_QUANTISATION, TQ2, psScanCtrl->componentList[2].uiQuantTable );
    REGIO_WRITE_FIELD( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_BE_QUANTISATION, TQ1, psScanCtrl->componentList[1].uiQuantTable );
    REGIO_WRITE_FIELD( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_BE_QUANTISATION, TQ0, psScanCtrl->componentList[0].uiQuantTable );
    psb_cmdbuf_rendec_write(cmdbuf, reg_value);

// CR_VEC_JPEG_BE_CONTROL
    reg_value = 0;
    REGIO_WRITE_FIELD( reg_value, MSVDX_VEC_JPEG, CR_VEC_JPEG_BE_CONTROL, RGB, 0 );
    psb_cmdbuf_rendec_write(cmdbuf, reg_value);

    psb_cmdbuf_rendec_end(cmdbuf);

}

static void psb__JPEG_write_kick(context_JPEG_p ctx, VASliceParameterBufferJPEG *slice_param)
{
    psb_cmdbuf_p cmdbuf = ctx->obj_context->cmdbuf;

    (void) slice_param; /* Unused for now */

    *cmdbuf->cmd_idx++ = CMD_COMPLETION;
}

static VAStatus psb__JPEG_process_slice(context_JPEG_p ctx,
        VASliceParameterBufferJPEG *slice_param,
        object_buffer_p obj_buffer)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;


    compile_huffman_tables();
//    ASSERT((obj_buffer->type == VASliceDataBufferType) || (obj_buffer->type == VAProtectedSliceDataBufferType));

    if ((slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_BEGIN) ||
        (slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_ALL)) {
        if (0 == slice_param->slice_data_size) {
            vaStatus = VA_STATUS_ERROR_UNKNOWN;
            DEBUG_FAILURE;
            return vaStatus;
        }
        ASSERT(!ctx->split_buffer_pending);

        /* Initialise the command buffer */
        /* TODO: Reuse current command buffer until full */
        psb_context_get_next_cmdbuf(ctx->obj_context);
#if 1
        psb__JPEG_FE_state(ctx);

#ifndef DE3_FIRMWARE
        psb_cmdbuf_lldma_write_bitstream(ctx->obj_context->cmdbuf,
                                         obj_buffer->psb_buffer,
                                         obj_buffer->psb_buffer->buffer_ofs + slice_param->slice_data_offset,
                                         slice_param->slice_data_size,
                                         slice_param->macroblock_offset,
                                         0);
#else

        psb_cmdbuf_dma_write_bitstream(ctx->obj_context->cmdbuf,
                                         obj_buffer->psb_buffer,
                                         obj_buffer->psb_buffer->buffer_ofs + slice_param->slice_data_offset,
                                         slice_param->slice_data_size,
                                         slice_param->macroblock_offset,
                                         0);
#endif

#endif
        if (slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_BEGIN) {
            ctx->split_buffer_pending = TRUE;
        }
    } else {
        ASSERT(ctx->split_buffer_pending);
        ASSERT(0 == slice_param->slice_data_offset);
        /* Create LLDMA chain to continue buffer */
        if (slice_param->slice_data_size) {
#ifndef DE3_FIRMWARE
            psb_cmdbuf_lldma_write_bitstream_chained(ctx->obj_context->cmdbuf,
                    obj_buffer->psb_buffer,
                    slice_param->slice_data_size);
#else
            psb_cmdbuf_dma_write_bitstream_chained(ctx->obj_context->cmdbuf,
                    obj_buffer->psb_buffer,
                    slice_param->slice_data_size);
#endif
        }
    }

    if ((slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_ALL) ||
        (slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_END)) {
        if (slice_param->slice_data_flag == VA_SLICE_DATA_FLAG_END) {
            ASSERT(ctx->split_buffer_pending);
        }

        psb__JPEG_set_operating_mode(ctx);

        psb__JPEG_set_reference_pictures(ctx);

        psb__JPEG_set_output_mode(ctx);

        psb__JPEG_set_ent_dec(ctx);

        psb__JPEG_set_register(ctx, slice_param);

        psb__JPEG_write_kick(ctx, slice_param);

        ctx->split_buffer_pending = FALSE;
        ctx->obj_context->video_op = psb_video_vld;
        ctx->obj_context->flags = 0;
 //       ctx->obj_context->first_mb = 0;
 //       ctx->obj_context->last_mb = ((ctx->picture_height_mb - 1) << 8) | (ctx->picture_width_mb - 1);

 //       *ctx->slice_first_pic_last = (ctx->obj_context->first_mb << 16) | (ctx->obj_context->last_mb);

        if (psb_context_submit_cmdbuf(ctx->obj_context)) {
            vaStatus = VA_STATUS_ERROR_UNKNOWN;
        }
    }
    return vaStatus;
}

static VAStatus psb__JPEG_process_slice_data(context_JPEG_p ctx, object_buffer_p obj_buffer)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    VASliceParameterBufferJPEG *slice_param;
    int buffer_idx = 0;
    unsigned int element_idx = 0;


 //   ASSERT((obj_buffer->type == VASliceDataBufferType) || (obj_buffer->type == VAProtectedSliceDataBufferType));

    ASSERT(ctx->pic_params);
    ASSERT(ctx->slice_param_list_idx);

    if (!ctx->pic_params) {
        /* Picture params missing */
        return VA_STATUS_ERROR_UNKNOWN;
    }
    if ((NULL == obj_buffer->psb_buffer) ||
        (0 == obj_buffer->size)) {
        /* We need to have data in the bitstream buffer */
        return VA_STATUS_ERROR_UNKNOWN;
    }

    while (buffer_idx < ctx->slice_param_list_idx) {
        object_buffer_p slice_buf = ctx->slice_param_list[buffer_idx];
        if (element_idx >= slice_buf->num_elements) {
            /* Move to next buffer */
            element_idx = 0;
            buffer_idx++;
            continue;
        }

        slice_param = (VASliceParameterBufferJPEG *) slice_buf->buffer_data;
        slice_param += element_idx;
        element_idx++;
        vaStatus = psb__JPEG_process_slice(ctx, slice_param, obj_buffer);
        if (vaStatus != VA_STATUS_SUCCESS) {
            DEBUG_FAILURE;
            break;
        }
    }
    ctx->slice_param_list_idx = 0;

    return vaStatus;
}

static VAStatus pnw_JPEG_BeginPicture(
    object_context_p obj_context)
{
    INIT_CONTEXT_JPEG

    if (ctx->pic_params) {
        free(ctx->pic_params);
        ctx->pic_params = NULL;
    }

    return VA_STATUS_SUCCESS;
}

static VAStatus pnw_JPEG_RenderPicture(
    object_context_p obj_context,
    object_buffer_p *buffers,
    int num_buffers)
{
    int i;
    INIT_CONTEXT_JPEG
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    for (i = 0; i < num_buffers; i++) {
        object_buffer_p obj_buffer = buffers[i];
        psb__dump_buffers_allkinds(obj_buffer);

        switch (obj_buffer->type) {
        case VAPictureParameterBufferType:
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "pnw_JPEG_RenderPicture got VAPictureParameterBuffer\n");
            vaStatus = psb__JPEG_process_picture_param(ctx, obj_buffer);
            DEBUG_FAILURE;
            break;

        case VAIQMatrixBufferType:
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "pnw_JPEG_RenderPicture got VAIQMatrixBufferType\n");
            vaStatus = psb__JPEG_process_iq_matrix(ctx, obj_buffer);
            DEBUG_FAILURE;
            break;

        case VAHuffmanTableBufferType:
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "pnw_JPEG_RenderPicture got VAIQMatrixBufferType\n");
            vaStatus = psb__JPEG_process_huffman_tables(ctx, obj_buffer);
            DEBUG_FAILURE;
            break;

        case VASliceParameterBufferType:
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "pnw_JPEG_RenderPicture got VASliceParameterBufferType\n");
            vaStatus = psb__JPEG_add_slice_param(ctx, obj_buffer);
            DEBUG_FAILURE;
            break;

        case VASliceDataBufferType:

            drv_debug_msg(VIDEO_DEBUG_GENERAL, "pnw_JPEG_RenderPicture got %s\n", SLICEDATA_BUFFER_TYPE(obj_buffer->type));
            vaStatus = psb__JPEG_process_slice_data(ctx, obj_buffer);
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

    return vaStatus;
}

static VAStatus pnw_JPEG_EndPicture(
    object_context_p obj_context)
{
    INIT_CONTEXT_JPEG

    if (psb_context_flush_cmdbuf(ctx->obj_context)) {
        return VA_STATUS_ERROR_UNKNOWN;
    }

    if (ctx->pic_params) {
        free(ctx->pic_params);
        ctx->pic_params = NULL;
    }

    return VA_STATUS_SUCCESS;
}

struct format_vtable_s pnw_JPEG_vtable = {
queryConfigAttributes:
    pnw_JPEG_QueryConfigAttributes,
validateConfig:
    pnw_JPEG_ValidateConfig,
createContext:
    pnw_JPEG_CreateContext,
destroyContext:
    pnw_JPEG_DestroyContext,
beginPicture:
    pnw_JPEG_BeginPicture,
renderPicture:
    pnw_JPEG_RenderPicture,
endPicture:
    pnw_JPEG_EndPicture
};
