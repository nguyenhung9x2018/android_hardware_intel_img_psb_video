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

 @File         msvdx_offsets.h

 @Title        MSVDX Offsets

 @Platform

 @Description

******************************************************************************/
#ifndef _MSVDX_OFFSETS_H_
#define _MSVDX_OFFSETS_H_

#include "msvdx_defs.h"

#define REG_MAC_OFFSET                          REG_MSVDX_DMAC_OFFSET
#define REG_MSVDX_CMDS_OFFSET           REG_MSVDX_CMD_OFFSET
#define REG_MSVDX_VEC_MPEG2_OFFSET      REG_MSVDX_VEC_OFFSET
#define REG_MSVDX_VEC_MPEG4_OFFSET      REG_MSVDX_VEC_OFFSET
#define REG_MSVDX_VEC_H264_OFFSET       REG_MSVDX_VEC_OFFSET
#define REG_MSVDX_VEC_VC1_OFFSET        REG_MSVDX_VEC_OFFSET
#define REG_MSVDX_CORE_OFFSET           REG_MSVDX_SYS_OFFSET
#define REG_DMAC_OFFSET                         REG_MSVDX_DMAC_OFFSET

#define RENDEC_REGISTER_OFFSET(__group__, __reg__ ) ( (__group__##_##__reg__##_OFFSET) + ( REG_##__group__##_OFFSET ) )

/* Not the best place for this */
#ifdef PLEASE_DONT_INCLUDE_REGISTER_BASE

/* This macro is used by KM gpu sim code */

#define REGISTER_OFFSET(__group__, __reg__ ) ( (__group__##_##__reg__##_OFFSET) )

#else

/* This is the macro used by UM Drivers - it included the Mtx memory offser to the msvdx redisters */

// #define REGISTER_OFFSET(__group__, __reg__ ) ( (__group__##_##__reg__##_OFFSET) + ( REG_##__group__##_OFFSET ) )
#define REGISTER_OFFSET(__group__, __reg__ ) ( (__group__##_##__reg__##_OFFSET) + ( REG_##__group__##_OFFSET ) + 0x04800000 )

#endif


#endif /*_MSVDX_OFFSETS_H_*/
