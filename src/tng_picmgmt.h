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
 *    Edward Lin <edward.lin@intel.com>
 *
 */
#ifndef _TNG_PICMGMT_H_
#define _TNG_PICMGMT_H_

#include "img_types.h"
#include "tng_hostcode.h"

typedef enum _pic_mgmt_type_
{
    IMG_PICMGMT_REF_TYPE=0,
    IMG_PICMGMT_GOP_STRUCT,
    IMG_PICMGMT_SKIP_FRAME,
    IMG_PICMGMT_EOS,
    IMG_PICMGMT_RC_UPDATE,
    IMG_PICMGMT_FLUSH,
    IMG_PICMGMT_QUANT,
} IMG_PICMGMT_TYPE;

/*!
 ***********************************************************************************
 *
 * Description        : PIC_MGMT - SetNextRefType parameters
 *
 ************************************************************************************/
typedef struct tag_IMG_PICMGMT_REF_DATA {
    IMG_FRAME_TYPE  eFrameType;                     //!< Type of the next reference frame (IDR, I, P)
} IMG_PICMGMT_REF_DATA;

/*!
 ***********************************************************************************
 *
 * Description        : PIC_MGMT - IMG_V_SetGopStructure parameters
 *
 ************************************************************************************/
typedef struct tag_IMG_PICMGMT_GOP_DATA {
    IMG_UINT8  ui8BFramePeriod;    //!< B-period
    IMG_UINT32 ui32IFramePeriod;   //!< I-period
} IMG_PICMGMT_GOP_DATA;

/*!
 ***********************************************************************************
 *
 * Description        : PIC_MGMT - IMG_V_SkipFrame parameters
 *
 ************************************************************************************/
typedef struct tag_IMG_PICMGMT_SKIP_DATA {
    IMG_BOOL8   b8Process;          //!< Process skipped frame (to update MV) ?
} IMG_PICMGMT_SKIP_DATA;

/*!
 ***********************************************************************************
 *
 * Description        : PIC_MGMT - IMG_V_EndOfStream parameters
 *
 ************************************************************************************/
typedef struct tag_IMG_PICMGMT_EOS_DATA {
    IMG_UINT32  ui32FrameCount; //!< Number of frames in the stream (incl. skipped)
} IMG_PICMGMT_EOS_DATA;

/*!
 ***********************************************************************************
 *
 * Description        : PIC_MGMT - IMG_V_RCUpdate parameters
 *
 ************************************************************************************/
typedef struct tag_IMG_PICMGMT_RC_UPDATE_DATA {
    IMG_UINT32  ui32BitsPerFrame;         //!< Number of bits in a frame
    IMG_UINT8   ui8VCMBitrateMargin;    //!< VCM bitrate margin
    IMG_UINT8   ui8VCMIFrameQP;        //!< VCM I frame QP
} IMG_PICMGMT_RC_UPDATE_DATA;

/*!
 ***********************************************************************************
 *
 * Description        : PIC_MGMT - IMG_V_FlushStream parameters
 *
 ************************************************************************************/
typedef struct tag_IMG_PICMGMT_FLUSH_STREAM_DATA
{
    IMG_UINT32  ui32FlushAtFrame;       //!< Frame Idx to flush the encoder
} IMG_PICMGMT_FLUSH_STREAM_DATA;

/*!
 ***********************************************************************************
 *
 * Description        : PIC_MGMT - IMG_V_SetCustomScalingValues parameters
 *
 ************************************************************************************/
typedef struct tag_IMG_PICMGMT_CUSTOM_QUANT_DATA
{
    IMG_UINT32  ui32Values;             //!< Address of custom quantization values
    IMG_UINT32  ui32Regs4x4Sp;      //!< Address of custom quantization register values for 4x4 Sp
    IMG_UINT32  ui32Regs8x8Sp;      //!< Address of custom quantization register values for 8x8 Sp
    IMG_UINT32  ui32Regs4x4Q;       //!< Address of custom quantization register values for 4x4 Q
    IMG_UINT32  ui32Regs8x8Q;       //!< Address of custom quantization register values for 8x8 Q
} IMG_PICMGMT_CUSTOM_QUANT_DATA;

/*!
 ***********************************************************************************
 *
 * Description        : Data Structure for PIC_MGMT command
 *
 ************************************************************************************/
typedef struct tag_IMG_PICMGMT_PARAMS
{
    IMG_PICMGMT_TYPE    eSubtype;       //!< Sybtype of the command (e.g. Skip Frame)
    union {	
        IMG_PICMGMT_REF_DATA     sRefType;          //!< Data for IMG_V_SetNextRefType
        IMG_PICMGMT_GOP_DATA	 sGopStruct;       //!< Data for IMG_V_SetGopStructure
        IMG_PICMGMT_SKIP_DATA    sSkipParams;     //!< Data for IMG_V_SkipFrame
        IMG_PICMGMT_EOS_DATA     sEosParams;     //!< Data for IMG_V_EndOfStream
        IMG_PICMGMT_RC_UPDATE_DATA      sRCUpdateData;      //!< Data for IMG_V_UpdateBitrate
        IMG_PICMGMT_FLUSH_STREAM_DATA   sFlushData;             //!< Data for IMG_V_FlushStream
        IMG_PICMGMT_CUSTOM_QUANT_DATA   sQuantData;           //!< Data for IMG_V_SetCustomScalingValues
    };
} IMG_PICMGMT_PARAMS;

/*!
***********************************************************************************
@Description        : PROVIDE_BUFFER - Type of the buffer
************************************************************************************/
typedef enum _buffer_type_ {
    IMG_BUFFER_SOURCE = 0,
    IMG_BUFFER_REF0,
    IMG_BUFFER_REF1,
    IMG_BUFFER_RECON,
    IMG_BUFFER_CODED,
} IMG_BUFFER_TYPE;

/*!
***********************************************************************************
@Description        : PROVIDE_BUFFER - Details of the source picture buffer
************************************************************************************/
typedef struct tag_IMG_BUFFER_SOURCE_DATA {
    IMG_UINT32      ui32PhysAddrYPlane_Field0;      //!< Source pic phys addr (Y plane, Field 0)
    IMG_UINT32      ui32PhysAddrUPlane_Field0;      //!< Source pic phys addr (U plane, Field 0)
    IMG_UINT32      ui32PhysAddrVPlane_Field0;      //!< Source pic phys addr (V plane, Field 0)
    IMG_UINT32      ui32PhysAddrYPlane_Field1;      //!< Source pic phys addr (Y plane, Field 1)
    IMG_UINT32      ui32PhysAddrUPlane_Field1;      //!< Source pic phys addr (U plane, Field 1)
    IMG_UINT32      ui32PhysAddrVPlane_Field1;      //!< Source pic phys addr (V plane, Field 1)
    IMG_UINT32      ui32HostContext;                        //!< Host context value
    IMG_UINT8       ui8DisplayOrderNum;                     //!< Number of frames in the stream (incl. skipped)
    IMG_UINT8       ui8SlotNum;                                     //!< Number of frames in the stream (incl. skipped)
} IMG_BUFFER_SOURCE_DATA;

/*!
***********************************************************************************
@Description        : PROVIDE_BUFFER - Details of the reference picture buffer
************************************************************************************/
typedef struct tag_IMG_BUFFER_REF_DATA {
    IMG_INT8    i8HeaderSlotNum;                        //!< Slot from which to read the slice header (-1 if none)
    IMG_BOOL8   b8LongTerm;                                     //!< Flag identifying the reference as long-term
} IMG_BUFFER_REF_DATA;

/*!
***********************************************************************************
@Description        : PROVIDE_BUFFER - Details of the coded buffer
************************************************************************************/
typedef struct tag_IMG_BUFFER_CODED_DATA {
    IMG_UINT32  ui32Size;           //!< Size of coded buffer in bytes
    IMG_UINT8   ui8SlotNum;             //!< Slot in firmware that this coded buffer should occupy
} IMG_BUFFER_CODED_DATA;

/*!
***********************************************************************************
@Description        : PROVIDE_BUFFER - Buffer details specific to the type of the buffer
************************************************************************************/
typedef union tag_IMG_BUFFER_DATA {
    IMG_BUFFER_SOURCE_DATA  sSource;                //!< Data for the source buffer
    IMG_BUFFER_REF_DATA     sRef;                   //!< Data for the reference buffer
    IMG_BUFFER_CODED_DATA   sCoded;                 //!< Data for the coded buffer
} IMG_BUFFER_DATA;

/*!
***********************************************************************************
@Description        : PROVIDE_BUFFER - Buffer details (general)
************************************************************************************/
typedef struct tag_IMG_BUFFER_PARAMS {
    IMG_BUFFER_TYPE eBufferType;            //!< Type of the buffer (e.g. Source)
    IMG_UINT32      ui32PhysAddr;
    IMG_BUFFER_DATA sData;                          //!< Supplementary data (depends on the type of the buffer)
} IMG_BUFFER_PARAMS;

IMG_UINT32 tng_skip_frame(context_ENC_p ctx, IMG_UINT8 ui32SlotIndex, IMG_BOOL bProcess);
IMG_UINT32 tng_end_of_stream(context_ENC_p ctx, IMG_UINT8 ui32SlotIndex, IMG_UINT32 ui32FrameCount);
IMG_UINT32 tng_send_codedbuf(context_ENC_p ctx, IMG_UINT32 ui32SlotIndex);
IMG_UINT32 tng_send_source_frame(context_ENC_p ctx, IMG_UINT32 ui32SlotIndex, IMG_UINT32 ui32DisplayOrder);
IMG_UINT32 tng_send_rec_frames(context_ENC_p ctx, IMG_BOOL bLongTerm, IMG_INT8 i8HeaderSlotNum);
IMG_UINT32 tng_send_ref_frames(context_ENC_p ctx, IMG_UINT32 ui32RefIndex, IMG_BOOL bLongTerm);

#endif //_TNG_PICMGMT_H_
