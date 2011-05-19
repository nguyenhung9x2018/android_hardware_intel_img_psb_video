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

#ifndef _PSB_HDMIEXTMODE_H_
#define _PSB_HDMIEXTMODE_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "va/va.h"
#include "va/va_backend.h"
#include "xf86drm.h"
#include "xf86drmMode.h"
#include "psb_output_android.h"

#define DRM_MODE_CONNECTOR_MIPI    15

typedef enum _psb_hdmi_mode {
    OFF,
    CLONE,
    EXTENDED_VIDEO,
    EXTENDED_DESKTOP,
} psb_hdmi_mode;

typedef struct _psb_extvideo_prop_s {
    psb_hdmi_mode ExtVideoMode;

    unsigned short ExtVideoMode_XRes;
    unsigned short ExtVideoMode_YRes;
    short ExtVideoMode_X_Offset;
    short ExtVideoMode_Y_Offset;
} psb_extvideo_prop_s, *psb_extvideo_prop_p;

typedef struct _psb_HDMIExt_info_s {
    /*MIPI infos*/
    uint32_t mipi_fb_id;
    /*hdmi infos*/
    uint32_t hdmi_connector_id;
    uint32_t hdmi_encoder_id;
    uint32_t hdmi_crtc_id;
    uint32_t hdmi_fb_id;
    drmModeConnection hdmi_connection;

    psb_hdmi_mode hdmi_mode;
    psb_extvideo_prop_p hdmi_extvideo_prop;
} psb_HDMIExt_info_s, *psb_HDMIExt_info_p;

VAStatus psb_HDMIExt_get_prop(psb_android_output_p output, unsigned short *xres, unsigned short *yres,
                              short *xoffset, short *yoffset);

psb_hdmi_mode psb_HDMIExt_get_mode(psb_android_output_p output);
VAStatus psb_HDMIExt_update(VADriverContextP ctx, psb_HDMIExt_info_p psb_HDMIExt_info);

psb_HDMIExt_info_p psb_HDMIExt_init(VADriverContextP ctx, psb_android_output_p output);
VAStatus psb_HDMIExt_deinit(psb_android_output_p output);

#endif /* _PSB_HDMIEXTMODE_H_*/
