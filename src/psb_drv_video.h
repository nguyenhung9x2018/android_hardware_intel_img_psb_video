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

#ifndef _PSB_DRV_VIDEO_H_
#define _PSB_DRV_VIDEO_H_

#include <pthread.h> /* POSIX threads headers */

#include <va/va_backend.h>
#include <va/va.h>
#include "object_heap.h"
#include "psb_def.h"
#include "psb_xvva.h"
#include "xf86drm.h"
#include "drm_sarea.h"
#include "psb_drm.h"
#include <stdint.h>
#include "hwdefs/dxva_fw_flags.h"
#ifndef ANDROID
#include <X11/Xlibint.h>
#include <X11/X.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/Xlib.h>
#else
#include <fourcc.h>
#define XID unsigned int
#endif
#include <wsbm/wsbm_pool.h>

/*
 * WORKAROUND_DMA_OFF_BY_ONE: LLDMA requests may access one additional byte which can cause
 * a MMU fault if the next byte after the buffer end is on a different page that isn't mapped.
 */
#define WORKAROUND_DMA_OFF_BY_ONE
#define FOURCC_NV12     (('2' << 24) + ('1' << 16) + ('V' << 8) + 'N')
#define XVIMAGE_NV12 \
   { \
        FOURCC_NV12, \
        XvYUV, \
        LSBFirst, \
        {'N','V','1','2', \
          0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
        12, \
        XvPlanar, \
        2, \
        0, 0, 0, 0, \
        8, 8, 8, \
        1, 2, 2, \
        1, 2, 2, \
        {'Y','U','V', \
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
        XvTopToBottom \
   }
#define FOURCC_RGBA 0x41424752
#define XVIMAGE_RGBA \
   { \
        FOURCC_RGBA, \
       XvRGB, \
        LSBFirst, \
        {'R','G','B','A', \
          0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
        32, \
        XvPacked, \
        1, \
        24, 0xFF0000, 0xFF00, 0xFF, \
        0, 0, 0, \
        0, 0, 0, \
        0, 0, 0, \
        {'R','G','B', \
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
        XvTopToBottom \
   }
#define FOURCC_XVVA     (('A' << 24) + ('V' << 16) + ('V' << 8) + 'X')
#define XVIMAGE_XVVA \
   { \
	FOURCC_XVVA, \
        XvYUV, \
	LSBFirst, \
	{'X','V','V','A', \
	  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
	12, \
	XvPlanar, \
	2, \
	0, 0, 0, 0, \
	8, 8, 8, \
	1, 2, 2, \
	1, 2, 2, \
	{'Y','U','V', \
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
	XvTopToBottom \
   }

#define PSB_MAX_PROFILES				12
#define PSB_MAX_ENTRYPOINTS				7
#define PSB_MAX_CONFIG_ATTRIBUTES		10
#define PSB_MAX_BUFFERTYPES			32

/* Max # of command submission buffers */
#define PSB_MAX_CMDBUFS				10	
#define LNC_MAX_CMDBUFS_ENCODE			4

#define PSB_SURFACE_DISPLAYING_F (0x1U<<0)
#define PSB_SURFACE_IS_FLAG_SET(flags, mask) (((flags)& PSB_SURFACE_DISPLAYING_F) != 0)

typedef struct object_config_s *object_config_p;
typedef struct object_context_s *object_context_p;
typedef struct object_surface_s *object_surface_p;
typedef struct object_buffer_s *object_buffer_p;
typedef struct object_image_s *object_image_p;
typedef struct object_subpic_s *object_subpic_p;
typedef struct format_vtable_s *format_vtable_p;
typedef struct psb_driver_data_s *psb_driver_data_p;

typedef enum _xvideo_port_type
{
    PSB_XVIDEO = 0,
    PSB_XVIDEO_HW_OVERLAY = 1
} xvideo_port_type;

struct psb_driver_data_s {
    struct object_heap_s	config_heap;
    struct object_heap_s	context_heap;
    struct object_heap_s	surface_heap;
    struct object_heap_s	buffer_heap;
    struct object_heap_s	image_heap;
    struct object_heap_s	subpic_heap;
    drm_handle_t		sarea_handle;
    drmAddress                  sarea_map;
    volatile struct drm_psb_sarea *psb_sarea;
    void *                      dri_priv;
    char *			bus_id;
    uint32_t                    dev_id;
    int				drm_fd;
    int				dri2;
    XID				context_id;
    drm_context_t		drm_context;
    drmLock			*drm_lock;
    int				contended_lock;
    pthread_mutex_t		drm_mutex;
    format_vtable_p		profile2Format[PSB_MAX_PROFILES][PSB_MAX_ENTRYPOINTS];
    void *			video_output;
    uint32_t			msvdx_context_base;
    int				video_sd_disabled;
    int				video_hd_disabled;
    void *                      camera_bo;
    uint32_t			camera_phyaddr;
    uint32_t			camera_size;
    void *                      rar_bo;
    uint32_t			rar_phyaddr;
    uint32_t			rar_size;
    void *                      rar_rd;

    int encode_supported;
    int decode_supported;
    int hd_encode_supported;
    int hd_decode_supported;

    int execIoctlOffset;
    int getParamIoctlOffset;
    
    struct _WsbmBufferPool *main_pool;
    struct _WsbmFenceMgr *fence_mgr;

    VASurfaceID cur_displaying_surface;
    VASurfaceID last_displaying_surface;
};

#define IS_MRST(driver_data) ((driver_data->dev_id & 0xFFFC) == 0x4100)

struct object_config_s {
    struct object_base_s base;
    VAProfile profile;
    VAEntrypoint entrypoint;
    VAConfigAttrib attrib_list[PSB_MAX_CONFIG_ATTRIBUTES];
    int attrib_count;
    format_vtable_p format_vtable;
};

struct object_context_s {
    struct object_base_s base;
    VAContextID context_id;
    VAConfigID config_id;
    int picture_width;
    int picture_height;
    int num_render_targets;
    VASurfaceID *render_targets;
    int va_flags;

    object_surface_p current_render_target;
    psb_driver_data_p driver_data;
    format_vtable_p format_vtable;
    void *format_data;
    struct psb_cmdbuf_s *cmdbuf_list[PSB_MAX_CMDBUFS];
    struct lnc_cmdbuf_s *lnc_cmdbuf_list[LNC_MAX_CMDBUFS_ENCODE];
    
    struct psb_cmdbuf_s *cmdbuf; /* Current cmd buffer */
    struct lnc_cmdbuf_s *lnc_cmdbuf;
    
    int cmdbuf_current;
    
    /* Buffers */
    object_buffer_p buffers_unused[PSB_MAX_BUFFERTYPES]; /* Linked lists (HEAD) of unused buffers for each buffer type */
    int buffers_unused_count[PSB_MAX_BUFFERTYPES]; /* Linked lists (HEAD) of unused buffers for each buffer type */
    object_buffer_p buffers_unused_tail[PSB_MAX_BUFFERTYPES]; /* Linked lists (TAIL) of unused buffers for each buffer type */
    object_buffer_p buffers_active[PSB_MAX_BUFFERTYPES]; /* Linked lists of active buffers for each buffer type */

    enum {
        psb_video_none = 0,
        psb_video_mc,
        psb_video_vld,
        psb_video_deblock
    } video_op;

    uint32_t operating_mode;
    uint32_t flags; /* See render flags below */
    uint32_t first_mb;
    uint32_t last_mb;

    uint32_t msvdx_context;
    
    /* Debug */
    uint32_t frame_count;
    uint32_t slice_count;
};

struct object_surface_s {
    struct object_base_s base;
    VASurfaceID surface_id;
    VAContextID context_id;
    int width;
    int height;
    struct psb_surface_s *psb_surface;
    void *subpictures;/* if not NULL, have subpicture information */
    unsigned int subpic_count; /* to ensure output have enough space for PDS & RAST */
    unsigned int derived_imgcnt; /* is the surface derived by a VAImage? */
    unsigned long display_timestamp; /* record the time point of put surface*/
};

struct object_buffer_s {
    struct object_base_s base;
    object_buffer_p ptr_next; /* Generic ptr for linked list */
    object_buffer_p *pptr_prev_next; /* Generic ptr for linked list */
    struct psb_buffer_s *psb_buffer;
    void *buffer_data;
    unsigned int size;
    unsigned int alloc_size;
    int max_num_elements;
    int num_elements;
    object_context_p context;
    VABufferType type;
    uint32_t last_used;
};

struct object_image_s {
    struct object_base_s base;
    VAImage image;
    unsigned int palette[16];
    int subpic_ref;
    VASurfaceID derived_surface;
};

struct object_subpic_s {
    struct object_base_s base;
    VASubpictureID subpic_id;

    VAImageID image_id;
    
    /* chromakey range */
    unsigned int chromakey_min;
    unsigned int chromakey_max;
    unsigned int chromakey_mask;

    /* global alpha */
    unsigned int global_alpha;

    /* flags */
    unsigned int flags; /* see below */

    void *surfaces; /* surfaces, associated with this subpicture */
};

struct format_vtable_s {
    void (*queryConfigAttributes) (
        VAProfile profile,
        VAEntrypoint entrypoint,
        VAConfigAttrib *attrib_list,
        int num_attribs
                                   );
    VAStatus (*validateConfig) (
        object_config_p obj_config
                                );
    VAStatus (*createContext) (
        object_context_p obj_context,
        object_config_p obj_config
                               );
    void (*destroyContext) (
        object_context_p obj_context
                            );
    VAStatus (*beginPicture) (
        object_context_p obj_context
                              );
    VAStatus (*renderPicture) (
        object_context_p obj_context,
        object_buffer_p *buffers,
        int num_buffers
                               );
    VAStatus (*endPicture) (
        object_context_p obj_context
                            );
};


#define psb__bounds_check(x, max)                                       \
    do { ASSERT(x < max); if (x >= max) x = max - 1; } while(0);

inline static char * buffer_type_to_string(int type)
{
    switch (type) {
    case VAPictureParameterBufferType: return "VAPictureParameterBufferType";
    case VAIQMatrixBufferType: return "VAIQMatrixBufferType";
    case VABitPlaneBufferType: return "VABitPlaneBufferType";
    case VASliceGroupMapBufferType: return "VASliceGroupMapBufferType";
    case VASliceParameterBufferType: return "VASliceParameterBufferType";
    case VASliceDataBufferType: return "VASliceDataBufferType";
    case VAProtectedSliceDataBufferType: return "VAProtectedSliceDataBufferType";
    case VAMacroblockParameterBufferType: return "VAMacroblockParameterBufferType";
    case VAResidualDataBufferType: return "VAResidualDataBufferType";
    case VADeblockingParameterBufferType: return "VADeblockingParameterBufferType";
    case VAImageBufferType: return "VAImageBufferType";
    case VAEncCodedBufferType: return "VAEncCodedBufferType";
    case VAEncSequenceParameterBufferType: return "VAEncSequenceParameterBufferType";
    case VAEncPictureParameterBufferType: return "VAEncPictureParameterBufferType";
    case VAEncSliceParameterBufferType: return "VAEncSliceParameterBufferType";
    default: return "UnknowBuffer";
    }
}

int  LOCK_HARDWARE(psb_driver_data_p driver_data);
int UNLOCK_HARDWARE(psb_driver_data_p driver_data);

#endif /* _PSB_DRV_VIDEO_H_ */
