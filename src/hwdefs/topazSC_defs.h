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

#define F_MASK(basename)  (MASK_##basename)
#define F_SHIFT(basename) (SHIFT_##basename)

#define F_ENCODE(val,basename)  (((val)<<(F_SHIFT(basename)))&(F_MASK(basename)))
#define F_ENCODE_MVEA(val,basename)  (((val)<<(F_SHIFT_MVEA(basename)))&(F_MASK_MVEA(basename)))

#define MVEA_LRB_SEARCH_HEIGHT 112
#define MVEA_LRB_SEARCH_WIDTH  144
#define MVEA_LRB_TOP_OFFSET    (((MVEA_LRB_SEARCH_HEIGHT/2)/16)*16) 


#define F_MASK_MVEA(basename)  (MASK_MVEA_##basename)   /*      MVEA    */
#define F_SHIFT_MVEA(basename) (SHIFT_MVEA_##basename)   /*     MVEA    */


#define MASK_MVEA_CR_IPE_GRID_FINE_SEARCH 0x00000C00
#define SHIFT_MVEA_CR_IPE_GRID_FINE_SEARCH 10
#define REGNUM_MVEA_CR_IPE_GRID_FINE_SEARCH 0x0200


#define MASK_MVEA_CR_IPE_GRID_SEARCH_SIZE 0x00000380
#define SHIFT_MVEA_CR_IPE_GRID_SEARCH_SIZE 7
#define REGNUM_MVEA_CR_IPE_GRID_SEARCH_SIZE 0x0200


#define MVEA_CR_IPE_CONTROL         0x0200
#define MASK_MVEA_CR_IPE_BLOCKSIZE  0x00000003
#define SHIFT_MVEA_CR_IPE_BLOCKSIZE 0
#define REGNUM_MVEA_CR_IPE_BLOCKSIZE 0x0200



#define MASK_MVEA_CR_IPE_ENCODING_FORMAT 0x00003000
#define SHIFT_MVEA_CR_IPE_ENCODING_FORMAT 12
#define REGNUM_MVEA_CR_IPE_ENCODING_FORMAT 0x0200


#define MASK_MVEA_CR_IPE_Y_CANDIDATE_NUM 0x0000003C
#define SHIFT_MVEA_CR_IPE_Y_CANDIDATE_NUM 2
#define REGNUM_MVEA_CR_IPE_Y_CANDIDATE_NUM 0x0200


#define MASK_MVEA_CR_IPE_Y_FINE_SEARCH 0x00000040
#define SHIFT_MVEA_CR_IPE_Y_FINE_SEARCH 6
#define REGNUM_MVEA_CR_IPE_Y_FINE_SEARCH 0x0200

#define MASK_MVEA_CR_JMCOMP_AC_ENABLE 0x00008000
#define SHIFT_MVEA_CR_JMCOMP_AC_ENABLE 15
#define REGNUM_MVEA_CR_JMCOMP_AC_ENABLE 0x0280


#define MB_START_OF_SLICE                       (1<<5)
#define MB_END_OF_SLICE                         (1<<4)
#define MB_END_OF_PICTURE                       (1<<6)
#define MB_END_OF_ROW                           (1<<7)
#define PARAMS_ABOVEL_VALID                     (1<<0)
#define PARAMS_ABOVE_VALID                      (1<<1)
#define PARAMS_ABOVER_VALID                     (1<<2)
#define PARAMS_LEFT_VALID                       (1<<3)#define VECTORS_ABOVE_LEFT_VALID        (1<<0)
#define VECTORS_ABOVE_VALID                     (1<<1)
#define VECTORS_LEFT_VALID                      (1<<2)
#define VECTORS_BELOW_VALID                     (1<<4)
#define VECTORS_2BELOW_VALID            (1<<5)
#define DO_INTRA_PRED                           (1<<6)

#define VECTORS_ABOVE_LEFT_VALID        (1<<0)

#define MVEA_CR_JMCOMP_CONTROL      0x0280
#define MASK_MVEA_CR_JMCOMP_MODE    0x00000003
#define SHIFT_MVEA_CR_JMCOMP_MODE   0
#define REGNUM_MVEA_CR_JMCOMP_MODE  0x0280

#define MASK_TOPAZ_VLC_CR_CODEC     0x00000003
#define SHIFT_TOPAZ_VLC_CR_CODEC    0

#define MASK_TOPAZ_VLC_CR_SLICE_CODING_TYPE 0x0000000C
#define SHIFT_TOPAZ_VLC_CR_SLICE_CODING_TYPE 2

#define MVEA_CR_SPE_ZERO_THRESH     0x0370

#define MVEA_CR_IPE_LAMBDA_TABLE    0x01F0
#define MASK_MVEA_CR_IPE_QPC_OR_DC_SCALE_LUMA_TABLE 0x000000FF
#define SHIFT_MVEA_CR_IPE_QPC_OR_DC_SCALE_LUMA_TABLE 0

#define MVEA_CR_IPE_MV_BIAS_TABLE   0x01F4

#define MVEA_CR_SPE_PRED_VECTOR_BIAS_TABLE 0x037C

#define MVEA_CR_SPE_INTRA16_BIAS_TABLE 0x0374

#define MVEA_CR_SPE_INTER_BIAS_TABLE 0x0378

#define MASK_MVEA_CR_IPE_ALPHA_OR_DC_SCALE_CHR_TABLE 0x0000FF00
#define SHIFT_MVEA_CR_IPE_ALPHA_OR_DC_SCALE_CHR_TABLE 8




