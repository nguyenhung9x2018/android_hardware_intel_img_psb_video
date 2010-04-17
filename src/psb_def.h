/*
 * Copyright (c) 2007 Intel Corporation. All Rights Reserved.
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
 */

#ifndef _PSB_DEF_H_
#define _PSB_DEF_H_

#include <assert.h>
#include <string.h>

/* #define VA_EMULATOR 1 */

/* #define DEBUG_TRACE  */
/* #define DEBUG_TRACE_VERBOSE */


#ifdef DEBUG_TRACE

#ifndef ASSERT
#define ASSERT	assert
#endif

#ifndef IMG_ASSERT
#define IMG_ASSERT  assert
#endif

#else /* DEBUG_TRACE */

#undef ASSERT
#undef IMG_ASSERT

#define ASSERT(x)
#define IMG_ASSERT(x)

#endif /* DEBUG_TRACE */

#ifndef FALSE
#define FALSE 	0
#endif

#ifndef TRUE
#define TRUE 	1
#endif

#ifdef ANDROID
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

#ifdef PSBVIDEO_LOG_ENABLE

#include <utils/Log.h>

#define psb__error_message(msg, ...) \
    __android_log_print(ANDROID_LOG_ERROR, "psb_video", "%s():%d: "msg, \
                        __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define psb__information_message(msg, ...) \
    __android_log_print(ANDROID_LOG_INFO, "psb_video", "%s():%d: "msg, \
                        __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define psb__trace_message(msg, ...) \
    __android_log_print(ANDROID_LOG_VERBOSE, "psb_video", "%s():%d: "msg, \
                        __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define DEBUG_FAILURE                                                   \
    ({                                                                  \
    if (vaStatus) {                                                     \
        __android_log_print(ANDROID_LOG_ERROR,                          \
                            "psb_video",                                \
                            "%s fails with '%s' at %s:%d\n",            \
                            __FUNCTION__, vaErrorStr(vaStatus),         \
                            __FILE__, __LINE__);                        \
    }                                                                   \
    })

#define DEBUG_FAILURE_RET                                               \
    ({                                                                  \
    if (ret) {                                                          \
        __android_log_print(ANDROID_LOG_ERROR,                          \
                           "psb_video",                                 \
                           "%s fails with '%s' at %s:%d\n",             \
                           __FUNCTION__, strerror(ret < 0 ? -ret : ret),\
                           __FILE__, __LINE__);                         \
    }                                                                   \
    })

#else

#define psb__error_message(msg, ...)
#define psb__information_message(msg, ...)
#define psb__trace_message(msg, ...)

#define DEBUG_FAILURE
#define DEBUG_FAILURE_RET

#endif /* PSBVIDEO_LOG_ENABLE */

#else

void psb__error_message(const char *msg, ...);
void psb__information_message(const char *msg, ...);
void psb__trace_message(const char *msg, ...);


#define DEBUG_FAILURE		while(vaStatus) {psb__information_message("%s fails with '%s' at %s:%d\n", __FUNCTION__, vaErrorStr(vaStatus), __FILE__, __LINE__);break;}
#define DEBUG_FAILURE_RET	while(ret)		{psb__information_message("%s fails with '%s' at %s:%d\n", __FUNCTION__, strerror(ret < 0 ? -ret : ret), __FILE__, __LINE__);break;}

#endif /* ANDROID */

#endif /* _PSB_DEF_H_ */
