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
 *    Fei Jiang <fei.jiang@intel.com>
 *
 */

#ifndef _PSB_TEXSTREAMING_H_
#define _PSB_TEXSTREAMING_H_
#include <va/va.h>
#include <va/va_backend.h>

#define BC_FOURCC(a,b,c,d) \
    ((unsigned long) ((a) | (b)<<8 | (c)<<16 | (d)<<24))

#define BC_PIX_FMT_NV12     BC_FOURCC('N', 'V', '1', '2') /*YUV 4:2:0*/
#define BC_PIX_FMT_UYVY     BC_FOURCC('U', 'Y', 'V', 'Y') /*YUV 4:2:2*/
#define BC_PIX_FMT_YUYV     BC_FOURCC('Y', 'U', 'Y', 'V') /*YUV 4:2:2*/
#define BC_PIX_FMT_RGB565   BC_FOURCC('R', 'G', 'B', 'P') /*RGB 5:6:5*/

typedef struct BC_Video_ioctl_package_TAG {
    int ioctl_cmd;
    int device_id;
    int inputparam;
    int outputparam;
} BC_Video_ioctl_package;

typedef struct bc_buf_ptr {
    unsigned int index;
    int size;
    unsigned long pa;
    unsigned long handle;
} bc_buf_ptr_t;

#define BC_Video_ioctl_fill_buffer                  0
#define BC_Video_ioctl_get_buffer_count     1
#define BC_Video_ioctl_get_buffer_phyaddr   2  /*get physical address by index*/
#define BC_Video_ioctl_get_buffer_index         3  /*get index by physical address*/
#define BC_Video_ioctl_request_buffers      4
#define BC_Video_ioctl_set_buffer_phyaddr   5
#define BC_Video_ioctl_release_buffer_device  6

enum BC_memory {
    BC_MEMORY_MMAP          = 1,
    BC_MEMORY_USERPTR       = 2,
};

/*
 * the following types are tested for fourcc in struct bc_buf_params_t
 *   NV12
 *   UYVY
 *   RGB565 - not tested yet
 *   YUYV
 */
typedef struct bc_buf_params {
    int count;              /*number of buffers, [in/out]*/
    int width;              /*buffer width in pixel, multiple of 8 or 32*/
    int height;             /*buffer height in pixel*/
    int stride;
    unsigned int fourcc;    /*buffer pixel format*/
    enum BC_memory type;
} bc_buf_params_t;

VAStatus psb_register_video_bcd(
    VADriverContextP ctx,
    int width,
    int height,
    int stride,
    int num_surfaces,
    VASurfaceID *surface_list
);

VAStatus psb_release_video_bcd(VADriverContextP ctx);

/*add for texture streaming end*/
#endif /*_PSB_TEXSTREAMING_H*/
