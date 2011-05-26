/*
 * INTEL CONFIDENTIAL
 * Copyright 2007 Intel Corporation. All Rights Reserved.
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

/*
 * Authors:
 *    Jason Hu <jason.hu@intel.com>
 */

#ifndef _PSB_OUTPUT_ANDROID_H_
#define _PSB_OUTPUT_ANDROID_H_


typedef struct _psb_android_output_s {
    /* information of output display */
    unsigned short screen_width;
    unsigned short screen_height;

    /* for memory heap base used by putsurface */
    unsigned char* heap_addr;

    /*HDMI extend video mode info*/
    void* psb_HDMIExt_info;
    unsigned int sf_composition; /* surfaceflinger compostion */
} psb_android_output_s, *psb_android_output_p;

#endif /*_PSB_OUTPUT_ANDROID_H_*/
