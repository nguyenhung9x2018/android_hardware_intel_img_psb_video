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
 *    Fei Jiang  <fei.jiang@intel.com>
 *    Austin Yuan <austin.yuan@intel.com>
 *
 */

#include "android/psb_gralloc.h"
#include <cutils/log.h>
#include <utils/threads.h>
#include <ui/PixelFormat.h>
#include <hardware/gralloc.h>
#include <system/graphics.h>
#include <hardware/hardware.h>

using namespace android;

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif

#define LOG_TAG "pvr_drv_video"

static hw_module_t const *module;
static gralloc_module_t *mAllocMod; /* get by force hw_module_t */

int gralloc_lock(buffer_handle_t handle,
                int usage, int left, int top, int width, int height,
                void** vaddr)
{
    int err;

    if (!mAllocMod) {
        LOGW("%s: gralloc module has not been initialized. Should initialize it first", __func__);
        if (gralloc_init()) {
            LOGE("%s: can't find the %s module", GRALLOC_HARDWARE_MODULE_ID);
            return -1;
        }
    }

    err = mAllocMod->lock(mAllocMod, handle, usage,
                          left, top, width, height,
                          vaddr);
    LOGV("gralloc_lock: handle is %lx, usage is %x, vaddr is %x.\n", handle, usage, *vaddr);

    if (err){
        LOGE("lock(...) failed %d (%s).\n", err, strerror(-err));
        return -1;
    } else {
        LOGV("lock returned with address %p\n", *vaddr);
    }

    return err;
}

int gralloc_unlock(buffer_handle_t handle)
{
    int err;

    if (!mAllocMod) {
        LOGW("%s: gralloc module has not been initialized. Should initialize it first", __func__);
        if (gralloc_init()) {
            LOGE("%s: can't find the %s module", GRALLOC_HARDWARE_MODULE_ID);
            return -1;
        }
    }

    err = mAllocMod->unlock(mAllocMod, handle);
    if (err) {
        LOGE("unlock(...) failed %d (%s)", err, strerror(-err));
        return -1;
    } else {
        LOGV("unlock returned\n");
    }

    return err;
}

int gralloc_init(void)
{
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err) {
        LOGE("FATAL: can't find the %s module", GRALLOC_HARDWARE_MODULE_ID);
        return -1;
    } else
        LOGD("hw_get_module returned\n");
    mAllocMod = (gralloc_module_t *)module;

    return 0;
}

