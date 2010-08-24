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

#ifndef _PSB_XVVA_H
#define _PSB_XVVA_H

#include <pthread.h>
#include <stdint.h>


#ifndef MAKEFOURCC

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                                  \
    ((unsigned long)(unsigned char) (ch0) | ((unsigned long)(unsigned char) (ch1) << 8) | \
    ((unsigned long)(unsigned char) (ch2) << 16) | ((unsigned long)(unsigned char) (ch3) << 24 ))

/* a few common FourCCs */
#define VA_FOURCC_AI44         0x34344149
#define VA_FOURCC_UYVY          0x59565955
#define VA_FOURCC_YUY2          0x32595559
#define VA_FOURCC_AYUV          0x56555941
#define VA_FOURCC_NV11          0x3131564e
#define VA_FOURCC_YV12          0x32315659
#define VA_FOURCC_P208          0x38303250
#define VA_FOURCC_IYUV          0x56555949
#define VA_FOURCC_I420          0x30323449

#endif

/* XvDrawable information */
#define XVDRAWABLE_NORMAL	0x00
#define XVDRAWABLE_PIXMAP	0x01
#define XVDRAWABLE_ROTATE_90	0x02
#define XVDRAWABLE_ROTATE_180	0x04
#define XVDRAWABLE_ROTATE_270	0x08
#define XVDRAWABLE_REDIRECT_WINDOW 0x10
#define XVDRAWABLE_SCALE	0x20

#define XVDRAWABLE_INVALID_DRAWABLE	0x8000

typedef struct _PsbAYUVSample8 {
    unsigned char     Cr;
    unsigned char     Cb;
    unsigned char     Y;
    unsigned char     Alpha;
} PsbAYUVSample8;

typedef struct _VaClipBox {
    short x;
    short y;
    unsigned short width;
    unsigned short height;
} VaClipBox;


struct _PsbVASurface {
    struct _PsbVASurface *next; /* next subpicture, only used by client */
    
    struct _WsbmBufferObject *bo;
    uint32_t bufid;
    uint64_t pl_flags; /* placement */
    uint32_t size;

    unsigned int fourcc;
    unsigned int planar;
    unsigned int width;
    unsigned int height;
    unsigned int bytes_pp;
    unsigned int stride;
    unsigned int pre_add;
    unsigned int reserved_phyaddr; /* for reserved memory, e.g. CI/RAR */

    unsigned int clear_color;

    unsigned int subpic_id; /* subpic id, only used by client */
    unsigned int subpic_flags;/* flags for subpictures 
                               * #define VA_SUBPICTURE_CHROMA_KEYING	0x0001
                               * #define VA_SUBPICTURE_GLOBAL_ALPHA	0x0002
                               */
    float global_alpha;
    unsigned int chromakey_min;
    unsigned int chromakey_max;
    unsigned int chromakey_mask;

    PsbAYUVSample8 *palette_ptr; /* point to image palette */
    union {
        uint32_t  palette[16]; /* used to pass palette to server */
        PsbAYUVSample8  constant[16]; /* server convert palette into SGX constants */
    };
    int subpic_srcx;
    int subpic_srcy;
    int subpic_srcw;
    int subpic_srch;

    int subpic_dstx;
    int subpic_dsty;
    int subpic_dstw;
    int subpic_dsth;

    /* only used by server side */
    unsigned int num_constant;
    unsigned int *constants;
    
    unsigned int mem_layout;
    unsigned int tex_fmt;
    unsigned int pack_mode;

    unsigned int fragment_start;
    unsigned int fragment_end;
};

typedef struct _PsbVASurface PsbVASurfaceRec;
typedef struct _PsbVASurface *PsbVASurfacePtr;


#ifndef VA_FRAME_PICTURE 

/* de-interlace flags for vaPutSurface */
#define VA_FRAME_PICTURE        0x00000000 
#define VA_TOP_FIELD            0x00000001
#define VA_BOTTOM_FIELD         0x00000002
/* 
 * clears the drawable with background color.
 * for hardware overlay based implementation this flag
 * can be used to turn off the overlay
 */
#define VA_CLEAR_DRAWABLE       0x00000008

/* color space conversion flags for vaPutSurface */
#define VA_SRC_BT601		0x00000010
#define VA_SRC_BT709		0x00000020

#endif /* end for _VA_X11_H_ */



#define PSB_SUBPIC_MAX_NUM	6
#define PSB_CLIPBOX_MAX_NUM	6

typedef struct _PsbXvVAPutSurface {
    uint32_t flags;/* #define VA_FRAME_PICTURE 0x00000000
                    * #define VA_TOP_FIELD     0x00000001
                    * #define VA_BOTTOM_FIELD  0x00000002
                    */
    unsigned int num_subpicture;
    unsigned int num_clipbox;

    PsbVASurfaceRec dst_srf; /* filled by Xserver */
    PsbVASurfaceRec src_srf; /* provided by VA client */
    PsbVASurfaceRec subpic_srf[PSB_SUBPIC_MAX_NUM];
    VaClipBox clipbox[PSB_CLIPBOX_MAX_NUM];
} PsbXvVAPutSurfaceRec, *PsbXvVAPutSurfacePtr;

#endif
