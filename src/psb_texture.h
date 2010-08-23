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
** psb_texture.h
** Login : <brady@luna.bj.intel.com>
** Started on  Wed Mar 31 14:40:52 2010 brady
** $Id$
** 
** Copyright (C) 2010 brady
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef   	PSB_TEXTURE_H_
# define   	PSB_TEXTURE_H_

#include "pvr2d.h"
#include <img_types.h>

#define DRI2_BACK_BUFFER_EXPORT_TYPE_BUFFERS 1
#define DRI2_BACK_BUFFER_EXPORT_TYPE_SWAPCHAIN 2

#define DRI2_FLIP_BUFFERS_NUM           3
#define DRI2_BLIT_BUFFERS_NUM           2
#define DRI2_MAX_BUFFERS_NUM            MAX( DRI2_FLIP_BUFFERS_NUM, DRI2_BLIT_BUFFERS_NUM )

typedef struct _psb_coeffs_
{
    signed char rY;
    signed char rU;
    signed char rV;
    signed char gY;
    signed char gU;
    signed char gV;
    signed char bY;
    signed char bU;
    signed char bV;
    unsigned char rShift;
    unsigned char gShift;
    unsigned char bShift;
    signed short rConst;
    signed short gConst;
    signed short bConst;
} psb_coeffs_s, *psb_coeffs_p;

typedef struct _sgx_psb_fixed32 {
    union {
        struct {
            unsigned short Fraction;
            short Value;
        };
        long ll;
    };
} sgx_psb_fixed32;

typedef struct _PVRDRI2BackBuffersExport_
{
    IMG_UINT32 ui32Type;
    //pixmap handles
    PVR2D_HANDLE hBuffers[3];

    IMG_UINT32 ui32BuffersCount;
    IMG_UINT32 ui32SwapChainID;
} PVRDRI2BackBuffersExport;

struct psb_texture_s {
    void *hPVR2DContext;

    struct _WsbmBufferObject *vaSrf;

    unsigned int video_transfermatrix;
    unsigned int src_nominalrange;
    unsigned int dst_nominalrange;

    uint32_t gamma0;
    uint32_t gamma1;
    uint32_t gamma2;
    uint32_t gamma3;
    uint32_t gamma4;
    uint32_t gamma5;

    sgx_psb_fixed32 brightness;
    sgx_psb_fixed32 contrast;
    sgx_psb_fixed32 saturation;
    sgx_psb_fixed32 hue;

    psb_coeffs_s coeffs;
#ifndef ANDROID
    uint32_t dri_init_flag;
    uint32_t current_blt_buffer;

    struct dri_drawable *dri_drawable;
    union dri_buffer *dri_buffer;

    PVRDRI2BackBuffersExport dri2_bb_export;
    PVR2DMEMINFO *blt_meminfo[DRI2_BLIT_BUFFERS_NUM];
    PVR2DMEMINFO *flip_meminfo[DRI2_FLIP_BUFFERS_NUM];
#endif
};

void psb_ctexture_init(VADriverContextP ctx);

void psb_ctexture_deinit(VADriverContextP ctx);

void blit_texture_to_buf(VADriverContextP ctx, unsigned char * data, int src_x, int src_y, int src_w,
			 int src_h, int dst_x, int dst_y, int dst_w, int dst_h,
			 int width, int height, int src_pitch, struct _WsbmBufferObject * src_buf,
			 unsigned int placement);


#endif 	    /* !PSB_TEXTURE_H_ */
