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

#ifndef _PSB_SURFACE_H_
#define _PSB_SURFACE_H_

#include <va/va.h>
#include "psb_buffer.h"
//#include "xf86mm.h"

/* MSVDX specific */
typedef enum
{
    STRIDE_352	= 0,
    STRIDE_720	= 1,
    STRIDE_1280	= 2,
    STRIDE_1920 = 3,
    STRIDE_512	= 4,
    STRIDE_1024	= 5,
    STRIDE_2048 = 6,
    STRIDE_4096 = 7,
    STRIDE_NA,
    STRIDE_UNDEFINED,
} psb_surface_stride_t;

typedef struct psb_surface_s *psb_surface_p;

struct psb_surface_s {
    struct psb_buffer_s buf;
    struct psb_buffer_s *in_loop_buf;
    struct psb_buffer_s *ref_buf;
    psb_surface_stride_t stride_mode;
    int	stride;
    unsigned int luma_offset;
    unsigned int chroma_offset;
    /* Used to store driver private data, e.g. decoder specific intermediate status data
     * extra_info[0-3]: used for decode
     * extra_info[4]: surface fourcc
     * extra_info[5]: surface skippeded or not for encode
     */
    int extra_info[6];	
    int size;
};

/*
 * Create surface
 */
VAStatus psb_surface_create( psb_driver_data_p driver_data,
                             int width, int height, int fourcc, int protected,
                             psb_surface_p psb_surface /* out */
                           );


VAStatus psb_surface_create_camera( psb_driver_data_p driver_data,
                             int width, int height, int stride, int size,
                             psb_surface_p psb_surface, /* out */
                             int is_v4l2,
                             unsigned int id_or_ofs
                                    );

/* id_or_ofs: it is frame ID or frame offset in camear device memory
 *     for CI frame: it it always frame offset currently
 *     for v4l2 buf: it is offset used in V4L2 buffer mmap
 * user_ptr: virtual address of user buffer.    
 */
VAStatus psb_surface_create_camera_from_ub( psb_driver_data_p driver_data,
                             int width, int height, int stride, int size,
                             psb_surface_p psb_surface, /* out */
                             int is_v4l2,
                             unsigned int id_or_ofs,
			     const unsigned long *user_ptr);

/*
 * Temporarily map surface and set all chroma values of surface to 'chroma'
 */   
VAStatus psb_surface_set_chroma( psb_surface_p psb_surface, int chroma );

/*
 * Destroy surface
 */   
void psb_surface_destroy( psb_surface_p psb_surface );

/*
 * Wait for surface to become idle
 */
VAStatus psb_surface_sync( psb_surface_p psb_surface );

/*
 * Return surface status
 */
VAStatus psb_surface_query_status( psb_surface_p psb_surface, VASurfaceStatus *status );

#endif /* _PSB_SURFACE_H_ */
