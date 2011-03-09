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

#ifndef _LLDMA_DEFS_H_
#define _LLDMA_DEFS_H_

typedef enum {
    LLDMA_TYPE_VLC_TABLE ,
    LLDMA_TYPE_BITSTREAM ,
    LLDMA_TYPE_RESIDUAL ,
    LLDMA_TYPE_RENDER_BUFF_MC,
    LLDMA_TYPE_RENDER_BUFF_VLD,

    LLDMA_TYPE_MPEG4_FESTATE_SAVE,
    LLDMA_TYPE_MPEG4_FESTATE_RESTORE,

    LLDMA_TYPE_H264_PRELOAD_SAVE,
    LLDMA_TYPE_H264_PRELOAD_RESTORE,

    LLDMA_TYPE_VC1_PRELOAD_SAVE,
    LLDMA_TYPE_VC1_PRELOAD_RESTORE,

    LLDMA_TYPE_MEM_SET,

} LLDMA_TYPE;

#endif /* _LLDMA_DEFS_H_ */
