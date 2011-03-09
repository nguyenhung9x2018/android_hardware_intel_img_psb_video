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

 @File         msvdx_rendec_mtx_slice_cntrl_reg_io2.h

 @Title        MSVDX Offsets

 @Platform     </b>\n

 @Description  </b>\n This file contains the
               MSVDX_RENDEC_MTX_SLICE_CNTRL_REG_IO2_H Defintions.

******************************************************************************/
#if !defined (__MSVDX_RENDEC_MTX_SLICE_CNTRL_REG_IO2_H__)
#define __MSVDX_RENDEC_MTX_SLICE_CNTRL_REG_IO2_H__

#ifdef __cplusplus
extern "C"
{
#endif


#define RENDEC_SLICE_INFO_SL_HDR_CK_START_OFFSET		(0x0000)

// RENDEC_SLICE_INFO     SL_HDR_CK_START     SL_NUM_SYMBOLS_LESS1
#define RENDEC_SLICE_INFO_SL_HDR_CK_START_SL_NUM_SYMBOLS_LESS1_MASK		(0x07FF0000)
#define RENDEC_SLICE_INFO_SL_HDR_CK_START_SL_NUM_SYMBOLS_LESS1_LSBMASK		(0x000007FF)
#define RENDEC_SLICE_INFO_SL_HDR_CK_START_SL_NUM_SYMBOLS_LESS1_SHIFT		(16)

// RENDEC_SLICE_INFO     SL_HDR_CK_START     SL_ROUTING_INFO
#define RENDEC_SLICE_INFO_SL_HDR_CK_START_SL_ROUTING_INFO_MASK		(0x00000018)
#define RENDEC_SLICE_INFO_SL_HDR_CK_START_SL_ROUTING_INFO_LSBMASK		(0x00000003)
#define RENDEC_SLICE_INFO_SL_HDR_CK_START_SL_ROUTING_INFO_SHIFT		(3)

// RENDEC_SLICE_INFO     SL_HDR_CK_START     SL_ENCODING_METHOD
#define RENDEC_SLICE_INFO_SL_HDR_CK_START_SL_ENCODING_METHOD_MASK		(0x00000007)
#define RENDEC_SLICE_INFO_SL_HDR_CK_START_SL_ENCODING_METHOD_LSBMASK		(0x00000007)
#define RENDEC_SLICE_INFO_SL_HDR_CK_START_SL_ENCODING_METHOD_SHIFT		(0)

#define RENDEC_SLICE_INFO_SL_HDR_CK_PARAMS_OFFSET		(0x0004)

// RENDEC_SLICE_INFO     SL_HDR_CK_PARAMS     SL_DATA
#define RENDEC_SLICE_INFO_SL_HDR_CK_PARAMS_SL_DATA_MASK		(0xFFFFFFFF)
#define RENDEC_SLICE_INFO_SL_HDR_CK_PARAMS_SL_DATA_LSBMASK		(0xFFFFFFFF)
#define RENDEC_SLICE_INFO_SL_HDR_CK_PARAMS_SL_DATA_SHIFT		(0)

#define RENDEC_SLICE_INFO_CK_HDR_OFFSET		(0x0008)

// RENDEC_SLICE_INFO     CK_HDR     CK_NUM_SYMBOLS_LESS1
#define RENDEC_SLICE_INFO_CK_HDR_CK_NUM_SYMBOLS_LESS1_MASK		(0x07FF0000)
#define RENDEC_SLICE_INFO_CK_HDR_CK_NUM_SYMBOLS_LESS1_LSBMASK		(0x000007FF)
#define RENDEC_SLICE_INFO_CK_HDR_CK_NUM_SYMBOLS_LESS1_SHIFT		(16)

// RENDEC_SLICE_INFO     CK_HDR     CK_START_ADDRESS
#define RENDEC_SLICE_INFO_CK_HDR_CK_START_ADDRESS_MASK		(0x0000FFF0)
#define RENDEC_SLICE_INFO_CK_HDR_CK_START_ADDRESS_LSBMASK		(0x00000FFF)
#define RENDEC_SLICE_INFO_CK_HDR_CK_START_ADDRESS_SHIFT		(4)

// RENDEC_SLICE_INFO     CK_HDR     CK_ENCODING_METHOD
#define RENDEC_SLICE_INFO_CK_HDR_CK_ENCODING_METHOD_MASK		(0x00000007)
#define RENDEC_SLICE_INFO_CK_HDR_CK_ENCODING_METHOD_LSBMASK		(0x00000007)
#define RENDEC_SLICE_INFO_CK_HDR_CK_ENCODING_METHOD_SHIFT		(0)

#define RENDEC_SLICE_INFO_SLICE_SEPARATOR_OFFSET		(0x000C)

// RENDEC_SLICE_INFO     SLICE_SEPARATOR     SL_SEP_SUFFIX
#define RENDEC_SLICE_INFO_SLICE_SEPARATOR_SL_SEP_SUFFIX_MASK		(0x00000007)
#define RENDEC_SLICE_INFO_SLICE_SEPARATOR_SL_SEP_SUFFIX_LSBMASK		(0x00000007)
#define RENDEC_SLICE_INFO_SLICE_SEPARATOR_SL_SEP_SUFFIX_SHIFT		(0)

#define RENDEC_SLICE_INFO_CK_GENERIC_OFFSET		(0x0010)

// RENDEC_SLICE_INFO     CK_GENERIC     CK_HW_CODE
#define RENDEC_SLICE_INFO_CK_GENERIC_CK_HW_CODE_MASK		(0x0000FFFF)
#define RENDEC_SLICE_INFO_CK_GENERIC_CK_HW_CODE_LSBMASK		(0x0000FFFF)
#define RENDEC_SLICE_INFO_CK_GENERIC_CK_HW_CODE_SHIFT		(0)



#ifdef __cplusplus
}
#endif

#endif /* __MSVDX_RENDEC_MTX_SLICE_CNTRL_REG_IO2_H__ */
