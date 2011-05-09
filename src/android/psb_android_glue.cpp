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
#include <cutils/log.h>

using namespace android;

sp<ISurface> isurface;
ISurface::BufferHeap mBufferHeap;

unsigned char* psb_android_registerBuffers(void** android_isurface, int pid, int width, int height)
{
    sp<MemoryHeapBase> heap;
    int framesize = width * height * 2;

    heap = new MemoryHeapBase(framesize);
    if (heap->heapID() < 0) {
        printf("Error creating frame buffer heap");
        return 0;
    }

    mBufferHeap = ISurface::BufferHeap(width, height, width, height, PIXEL_FORMAT_RGB_565, heap);

    isurface = static_cast<ISurface*>(*android_isurface);

    isurface->registerBuffers(mBufferHeap);

    return static_cast<uint8_t*>(mBufferHeap.heap->base());
}

void psb_android_postBuffer(int offset)
{
    if (isurface.get())
        isurface->postBuffer(offset);
}

//called in DestroySurfaces
void psb_android_clearHeap()
{
    if (isurface.get()) {
        isurface->unregisterBuffers();
        mBufferHeap.heap.clear();
    }
}

int psb_android_register_isurface(void** android_isurface, int bcd_id, int srcw, int srch)
{
    if (isurface != *android_isurface) {
        isurface = static_cast<ISurface*>(*android_isurface);
        if (isurface.get()) {
            isurface->createTextureStreamSource();
            LOGD("In psb_android_register_isurface: buffer_device_id is %d.\n", bcd_id);
            isurface->setTextureStreamID(bcd_id);
            return 0;
        } else {
            return -1;
        }
    }
    return 0;
}

void psb_android_texture_streaming_set_texture_dim(unsigned short srcw,
        unsigned short srch)
{
    static short saved_srcw, saved_srch;
    if(isurface.get() &&
       ((saved_srcw != srcw) || (saved_srch != srch))) {
#if 0
        /*
        Resolve issue - Green line in the bottom of display while video is played
        This issue is caused by the buffer size larger than video size and texture linear filtering.
        The pixels of last line will computed from the pixels out of video picture
        */
        if((srch  & 0x1f) == 0)
            isurface->setTextureStreamDim(srcw, srch);
        else
            isurface->setTextureStreamDim(srcw, srch - 1);
#else
        LOGD("In psb_android_texture_streaming_set_texture_dim: srcw is %d, srch is %d.\n", srcw, srch);
        /*surface flinger will do the upper height correction*/
        isurface->setTextureStreamDim(srcw, srch);
#endif
        saved_srcw = srcw;
        saved_srch = srch;
    }
}

void psb_android_texture_streaming_set_crop(short srcx,
        short srcy,
        unsigned short srcw,
        unsigned short srch)
{
    static short saved_srcx, saved_srcy, saved_srcw, saved_srch;
    if(isurface.get() &&
       ((saved_srcx != srcx) || (saved_srcy != srcy) || (saved_srcw != srcw) || (saved_srch != srch))) {
        /*assume crop will only be called from app layer*/
        isurface->setTextureStreamClipRect(srcx, srcy, srcw, srch);
        saved_srcx = srcx;
        saved_srcy = srcy;
        saved_srcw = srcw;
        saved_srch = srch;
    }
}

void psb_android_texture_streaming_set_blend(short destx,
        short desty,
        unsigned short destw,
        unsigned short desth,
        unsigned int background_color,
        unsigned int blend_color,
        unsigned short blend_mode)
{
    static unsigned short saved_destx, saved_desty, saved_destw, saved_desth;
    static unsigned int saved_background_color, saved_blend_color;
    static int saved_blend_mode = -1;
    unsigned short bg_red, bg_green, bg_blue, bg_alpha;
    unsigned short blend_red, blend_green, blend_blue, blend_alpha;

    if (saved_background_color != background_color) {
        bg_alpha = (background_color & 0xff000000) >> 24;
        bg_red = (background_color & 0xff0000) >> 16;
        bg_green = (background_color & 0xff00) >> 8;
        bg_blue = background_color & 0xff;
        saved_background_color = background_color;
    }

    if (saved_blend_color != blend_color) {
        blend_alpha = (blend_color & 0xff000000) >> 24;
        blend_red = (blend_color & 0xff0000) >> 16;
        blend_green = (blend_color & 0xff00) >> 8;
        blend_blue = blend_color & 0xff;
        saved_blend_color = blend_color;
    }

    if (isurface.get()) {
        if ((saved_destx != destx) || (saved_desty != desty) || (saved_destw != destw) || (saved_desth != desth)) {
            isurface->setTextureStreamPosRect(destx, desty, destw, desth);
            saved_destx = destx;
            saved_desty = desty;
            saved_destw = destw;
            saved_desth = desth;
        }
        if (saved_background_color != background_color)
            isurface->setTextureStreamBorderColor(bg_red, bg_green, bg_blue, bg_alpha);
        if (saved_blend_color != blend_color)
            isurface->setTextureStreamVideoColor(blend_red, blend_green, blend_blue, blend_alpha);
        if (saved_blend_mode != blend_mode) {
            isurface->setTextureStreamBlendMode(blend_mode);
            saved_blend_mode = blend_mode;
        }
    }
}

void psb_android_texture_streaming_display(int buffer_index)
{
    if (isurface.get())
        isurface->displayTextureStreamBuffer(buffer_index);
}

void psb_android_texture_streaming_destroy()
{
    if (isurface.get())
        isurface->destroyTextureStreamSource();
}

int psb_android_fallback_overlay()
{
    if (isurface.get()) {
	uint32_t pm = isurface->getVideoPostMethod();
	if (pm == ISurfaceComposer::eVideoPostOverlay) {
	    return 1;
	}
    }

    return 0;
}
