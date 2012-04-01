/*
 * Copyright (c) 2011 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Zhaohan Ren  <zhaohan.ren@intel.com>
 *    Jiang Fei  <jiang.fei@intel.com>
 *    Binglin Chen <binglin.chen@intel.com>
 *
 */


#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <surfaceflinger/Surface.h>
#include <surfaceflinger/ISurface.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <binder/MemoryHeapBase.h>
#include "psb_android_glue.h"
#include "psb_texstreaming.h"
#include "psb_output_android.h"
#include <cutils/log.h>
#include <ui/Rect.h>
#include <system/window.h>
#include <system/graphics.h>
#include "display/MultiDisplayClient.h"
#include "display/MultiDisplayType.h"

using namespace android;

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif

#define LOG_TAG "pvr_drv_video"

sp<ISurface> isurface;

void initMDC(void* output) {
    psb_android_output_p android_output = (psb_android_output_p)output;

    if(android_output->mMDClient == NULL) {
        LOGV("%s: new MultiDisplayClient", __func__);
        android_output->mMDClient = new MultiDisplayClient();
    }
}

void deinitMDC(void* output) {
    psb_android_output_p android_output = (psb_android_output_p)output;

    if (android_output->mMDClient) {
        LOGV("%s: delete MultiDisplayClient", __func__);
        delete (MultiDisplayClient *)(android_output->mMDClient);
        android_output->mMDClient = NULL;
    }
}

unsigned int update_forced;

int psb_android_register_isurface(void** android_isurface, int bcd_id, int srcw, int srch)
{
    return 0;
}

void psb_android_texture_streaming_set_texture_dim(unsigned short srcw,
        unsigned short srch)
{
}

void psb_android_texture_streaming_set_crop(short srcx,
        short srcy,
        unsigned short srcw,
        unsigned short srch)
{
}


void psb_android_texture_streaming_set_rotate(int rotate)
{
}

void psb_android_texture_streaming_set_blend(short destx,
        short desty,
        unsigned short destw,
        unsigned short desth,
        unsigned int background_color,
        unsigned int blend_color,
        unsigned short blend_mode)
{
}

void psb_android_texture_streaming_set_background_color(unsigned int background_color)
{
}

void psb_android_texture_streaming_display(int buffer_index)
{
}

void psb_android_texture_streaming_resetParams()
{
}

void psb_android_texture_streaming_destroy()
{
}

int psb_android_surfaceflinger_status(void** android_isurface, int *sf_compositioin, int *rotation, int *widi)
{
    return 0;
}

void psb_android_get_destbox(short* destx, short* desty, unsigned short* destw, unsigned short* desth)
{
}

int psb_android_surfaceflinger_rotate(void* native_window, int *rotation)
{
    sp<ANativeWindow> mNativeWindow = static_cast<ANativeWindow*>(native_window);
    int err, transform_hint;

    if (mNativeWindow.get()) {
        err = mNativeWindow->query(mNativeWindow.get(), NATIVE_WINDOW_TRANSFORM_HINT, &transform_hint);
        if (err != 0) {
            LOGE("%s: NATIVE_WINDOW_TRANSFORM_HINT query failed", __func__);
            return -1;
        }
        switch (transform_hint) {
        case HAL_TRANSFORM_ROT_90:
            *rotation = 1;
            break;
        case HAL_TRANSFORM_ROT_180:
            *rotation = 2;
            break;
        case HAL_TRANSFORM_ROT_270:
            *rotation = 3;
            break;
        default:
            *rotation = 0; 
        }
    }
    return 0;
}

int psb_android_is_extvideo_mode(void* output) {
    psb_android_output_p android_output = (psb_android_output_p)output;
    MultiDisplayClient* mMDClient = (MultiDisplayClient *)android_output->mMDClient;

    if (!android_output->mInitialized_mdclient) {
        initMDC(output);
        android_output->mInitialized_mdclient = 1;
    }

    if (mMDClient != NULL) {
        if (mMDClient->getMode() == MDS_HDMI_EXT) {
            return 1;
        }
    }
    return 0;
}
