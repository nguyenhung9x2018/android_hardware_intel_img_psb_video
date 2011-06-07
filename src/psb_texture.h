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


/*
 * Authors:
 *    Binglin Chen <binglin.chen@intel.com>
 *    Zhaohan Ren  <zhaohan.ren@intel.com>
 *    Shengquan Yuan  <shengquan.yuan@intel.com>
 *
 */

#ifndef         PSB_TEXTURE_H_
# define        PSB_TEXTURE_H_

#include "pvr2d.h"
#include <img_types.h>

#define DRI2_BACK_BUFFER_EXPORT_TYPE_BUFFERS 1
#define DRI2_BACK_BUFFER_EXPORT_TYPE_SWAPCHAIN 2

#define DRI2_FLIP_BUFFERS_NUM           2
#define DRI2_BLIT_BUFFERS_NUM           2
#define DRI2_MAX_BUFFERS_NUM            MAX( DRI2_FLIP_BUFFERS_NUM, DRI2_BLIT_BUFFERS_NUM )
#define VIDEO_BUFFER_NUM                20


typedef struct _psb_coeffs_ {
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

typedef struct _PVRDRI2BackBuffersExport_ {
    IMG_UINT32 ui32Type;
    //pixmap handles
    PVR2D_HANDLE hBuffers[3];

    IMG_UINT32 ui32BuffersCount;
    IMG_UINT32 ui32SwapChainID;
} PVRDRI2BackBuffersExport;

struct psb_texture_s {
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

    uint32_t update_coeffs;
    PVRDRI2BackBuffersExport dri2_bb_export;
    PVRDRI2BackBuffersExport extend_dri2_bb_export;

    /* struct dri_drawable *extend_dri_drawable; */
    /* struct dri_drawable *dri_drawable; */
    void *extend_dri_drawable;
    void *dri_drawable;

    uint32_t dri_init_flag;
    uint32_t extend_dri_init_flag;
    uint32_t adjust_window_flag;
    uint32_t current_blt_buffer;

    uint32_t extend_current_blt_buffer;
    uint32_t destw_save;
    uint32_t desth_save;
    uint32_t drawable_update_flag; /* drawable resize or switch between window <==> pixmap */
    uint32_t local_rotation_save;
    uint32_t extend_rotation_save;

    PVR2DMEMINFO *pal_meminfo[6];    
    PVR2DMEMINFO *blt_meminfo_pixmap;
    PVR2DMEMINFO *blt_meminfo[DRI2_BLIT_BUFFERS_NUM];
    PVR2DMEMINFO *flip_meminfo[DRI2_FLIP_BUFFERS_NUM];
    PVR2DMEMINFO *extend_blt_meminfo[DRI2_BLIT_BUFFERS_NUM];
};

int psb_ctexture_init(VADriverContextP ctx);

void psb_ctexture_deinit(VADriverContextP ctx);

void blit_texture_to_buf(VADriverContextP ctx, unsigned char * data, int src_x, int src_y, int src_w,
                         int src_h, int dst_x, int dst_y, int dst_w, int dst_h,
                         int width, int height, int src_pitch, struct _WsbmBufferObject * src_buf,
                         unsigned int placement);

void psb_putsurface_textureblit(
    VADriverContextP ctx, unsigned char *dst, VASurfaceID surface, int src_x, int src_y, int src_w,
    int src_h, int dst_x, int dst_y, int dst_w, int dst_h, unsigned int subtitle,
    int width, int height,
    int src_pitch, struct _WsbmBufferObject * src_buf,
    unsigned int placement, int wrap_dst);

#endif      /* !PSB_TEXTURE_H_ */
