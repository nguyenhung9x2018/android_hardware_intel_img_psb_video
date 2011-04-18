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
 *    Waldo Bastian <waldo.bastian@intel.com>
 *
 */

#ifndef _PSB_DEF_H_
#define _PSB_DEF_H_

#include <assert.h>
#include <string.h>

/* #define VA_EMULATOR 1 */

/* #define DEBUG_TRACE */
/* #define DEBUG_TRACE_VERBOSE */


#ifdef DEBUG_TRACE

#ifndef ASSERT
#define ASSERT  assert
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
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif


void psb__error_message(const char *msg, ...);
void psb__information_message(const char *msg, ...);
void psb__trace_message(const char *msg, ...);


#define DEBUG_FAILURE           while(vaStatus) {psb__information_message("%s fails with '%d' at %s:%d\n", __FUNCTION__, vaStatus, __FILE__, __LINE__);break;}
#define DEBUG_FAILURE_RET       while(ret)              {psb__information_message("%s fails with '%s' at %s:%d\n", __FUNCTION__, strerror(ret < 0 ? -ret : ret), __FILE__, __LINE__);break;}


#ifndef VA_FOURCC_YV16
#define VA_FOURCC_YV16 0x36315659
#endif
#endif /* _PSB_DEF_H_ */
