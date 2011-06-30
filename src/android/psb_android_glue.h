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
 *    Zhaohan Ren  <zhaohan.ren@intel.com>
 *
 */


#ifdef __cplusplus
extern "C" {
#endif

    unsigned char* psb_android_registerBuffers(void** surface, int pid, int width, int height);

    void psb_android_postBuffer(int offset);

    void psb_android_clearHeap();

    void psb_android_texture_streaming_display(int buffer_index);

    void psb_android_texture_streaming_set_texture_dim(unsigned short srcw,
            unsigned short srch);

    void psb_android_texture_streaming_set_crop(short srcx,
            short srcy,
            unsigned short srcw,
            unsigned short srch);

    void psb_android_texture_streaming_set_blend(short destx,
            short desty,
            unsigned short destw,
            unsigned short desth,
            unsigned int border_color,
            unsigned int blend_color,
            unsigned short blend_mode);

    void psb_android_texture_streaming_destroy();

    int psb_android_register_isurface(void** surface, int bcd_id, int srcw, int srch);
    int psb_android_surfaceflinger_status(void** surface, int *sf_compostion, int *rotation, int *widi);

    void psb_android_get_destbox(short* destx, short* desty, unsigned short* destw, unsigned short* desth);
    int psb_android_dynamic_source_init(void** android_isurface, int bcd_id, uint32_t srcw, uint32_t srch, uint32_t stride);
    void psb_android_dynamic_source_display(int buffer_index, int hdmi_mode);
    void psb_android_dynamic_source_destroy();

#ifdef __cplusplus
}
#endif
