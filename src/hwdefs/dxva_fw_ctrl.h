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

/******************************************************************************

 @File         dxva_fw_ctrl.h

 @Title        Dxva Firmware Control Allocation Commands

 @Platform

 @Description  Defined commands that may be placed int the Control Allocation

******************************************************************************/
#ifndef _DXVA_FW_CTRL_H_
#define _DXVA_FW_CTRL_H_


#define CMD_MASK						(0xF0000000)

/* No Operation */
#define CMD_NOP							(0x00000000)

/* Register Value Pair Block */
#define CMD_REGVALPAIR_WRITE			(0x10000000)
#define CMD_REGVALPAIR_COUNT_MASK		(0x000FFFFF)
#define CMD_REGVALPAIR_COUNT_SHIFT		(0)

#define CMD_REGVALPAIR_FLAG_MB_LAYER	(0x00100000)
#define CMD_REGVALPAIR_FLAG_HL_LAYER	(0x00200000)
#define CMD_REGVALPAIR_FLAG_PRELOAD		(0x00400000)
#define CMD_REGVALPAIR_FLAG_VC1PATCH            (0x00800000)

#define CMD_REGVALPAIR_FORCE_MASK		(0x08000000)

/* Rendec Write Block */
#define CMD_RENDEC_WRITE				(0x20000000)
#define CMD_RENDEC_BLOCK				(0x50000000)
#define CMD_RENDEC_COUNT_MASK			(0x000FFFFF)
#define CMD_RENDEC_COUNT_SHIFT			(0)

/* Rendec Block */
#define CMD_RENDEC_BLOCK_FLAG_VC1_CMD_PATCH     (0x01000000)
#define CMD_RENDEC_BLOCK_FLAG_VC1_BE_PATCH      (0x02000000)
#define CMD_RENDEC_BLOCK_FLAG_VC1_SP_PATCH      (0x04000000)
#define CMD_RENDEC_BLOCK_FLAG_VC1_IC_PATCH      (0x08000000)

/* Command Allocation temination Commands */
#define CMD_COMPLETION					(0x60000000)

/* Use this to notify mxt of the context */
#define CMD_HEADER					(0x70000000)
#define CMD_HEADER_CONTEXT_MASK		(0x0fffffff)

#define CMD_CONDITIONAL_SKIP		(0x80000000)

#define CMD_HEADER_VC1				(0x90000000)

#define CMD_PARSE_HEADER                                        (0xF0000000)
#define CMD_PARSE_HEADER_NEWSLICE                       (0x00000001)


typedef struct _RENDER_BUFFER_HEADER_VC1_TAG {
    IMG_UINT32 ui32Cmd;
    IMG_UINT32 ui32RangeMappingBase[2]; /* Store flags in bottom bits of [0] */
    IMG_UINT32 ui32SliceParams;
    union {
        struct	_LLDMA_VC1_ {
            IMG_UINT32		ui32PreloadSave;
            IMG_UINT32		ui32PreloadRestore;
        }		LLDMA_VC1;
    }	ui32LLDMAPointers;

} RENDER_BUFFER_HEADER_VC1;

typedef struct _RENDER_BUFFER_HEADER_TAG {
    IMG_UINT32 ui32Cmd;
    IMG_UINT32 ui32Reserved; /* used as ui32SliceParams in MPEG4 */
    union {
        struct	_LLDMA_MPEG4_ {
            IMG_UINT32		ui32FEStatesSave;
            IMG_UINT32		ui32FEStatesRestore;
        }		LLDMA_MPEG4;

        struct _LLDMA_H264_ {
            IMG_UINT32		ui32PreloadSave;
            IMG_UINT32		ui32PreloadRestore;

        }		LLDMA_H264;
    }	ui32LLDMAPointers;

} RENDER_BUFFER_HEADER;

typedef struct _PARSE_HEADER_CMD_TAG {
    IMG_UINT32      ui32Cmd;
    IMG_UINT32      ui32SeqHdrData;
    IMG_UINT32      ui32PicDimensions;
    IMG_UINT32      ui32BitplaneAddr[3];
    IMG_UINT32      ui32VLCTableAddr;
    IMG_UINT32      ui32ICParamData[2];
} PARSE_HEADER_CMD;

/* Linked list DMA Command */
#define CMD_LLDMA					(0xA0000000)
#define CMD_SLLDMA					(0xC0000000)		/* Syncronose LLDMA */
typedef struct {
    IMG_UINT32 ui32CmdAndDevLinAddr;
} LLDMA_CMD;

/* Shift Register Setup Command */
#define CMD_SR_SETUP				(0xB0000000)
#define CMD_ENABLE_RBDU_EXTRACTION		(0x00000001)
typedef struct {
    IMG_UINT32 ui32Cmd;
    IMG_UINT32 ui32BitstreamOffsetBits;
    IMG_UINT32 ui32BitstreamSizeBytes;
} SR_SETUP_CMD;

/* Next Segment Command */
#define CMD_NEXT_SEG				(0xD0000000)	/* Also Syncronose */

#endif
