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

#ifndef _PNW_JPEG_H_
#define _PNW_JPEG_H_

#include "psb_drv_video.h"

#define PNW_JPEG_MAX_SCAN_NUM 7
extern struct format_vtable_s pnw_JPEG_vtable;
extern VAStatus pnw_jpeg_AppendMarkers(object_context_p obj_context, void *raw_coded_buf);

#endif /* _PNW_MPEG4ES_H_ */
