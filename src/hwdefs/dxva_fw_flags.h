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

 @File         dxva_fw_flags.h

 @Title        Dxva Firmware Message Flags

 @Platform

 @Description

******************************************************************************/
#ifndef _VA_FW_FLAGS_H_
#define _VA_FW_FLAGS_H_

/* Flags */

#define FW_VA_RENDER_IS_FIRST_SLICE                                             0x00000001
#define FW_VA_RENDER_IS_H264_MBAFF                                              0x00000002
#define FW_VA_RENDER_IS_LAST_SLICE                                              0x00000004
#define FW_VA_RENDER_IS_TWO_PASS_DEBLOCK                                        0x00000008

#define REPORT_STATUS                                                                           0x00000010

#define FW_VA_RENDER_IS_VLD_NOT_MC                                              0x00000800

#define FW_ERROR_DETECTION_AND_RECOVERY                                         0x00000100
#define FW_VA_RENDER_NO_RESPONCE_MSG                                            0x00002000  /* Cause no responce message to be sent, and no interupt generation on successfull completion */
#define FW_VA_RENDER_HOST_INT                                                           0x00004000
#define FW_VA_RENDER_VC1_BITPLANE_PRESENT                                   0x00008000

#endif /*_VA_FW_FLAGS_H_*/
