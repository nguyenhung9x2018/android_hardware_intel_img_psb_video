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

#ifndef _PSB_OVERLAY_H_
#define _PSB_OVERLAY_H_

#define USE_OVERLAY 1
#define USE_DISPLAY_C_SPRITE 0

/*
 * NOTE: Destination keying when enabled forces the overlay surface
 * Z order to be below the primary display. Pixels that match the key
 * value become transparent and the overlay becomes visible at that
 * pixel.
 */
#define USE_DCLRK 1
/*
 * NOTE: This is only for media player output
 */
#define USE_CLIP_FUNC 0
#define USE_SCALE_FUNC 1
#define USE_ROTATION_FUNC 1

#define Success 0

/* FIXME this will be removed later after using pvr2d */
#if 1
#define RR_Rotate_0         1
#define RR_Rotate_90        2
#define RR_Rotate_180       4
#define RR_Rotate_270       8
#endif

#define OV_HUE_DEFAULT_VALUE   0
#define OV_HUE_MIN            -30
#define OV_HUE_MAX             30

#define OV_BRIGHTNESS_DEFAULT_VALUE   0
#define OV_BRIGHTNESS_MIN            -50
#define OV_BRIGHTNESS_MAX             50

#define OV_CONTRAST_DEFAULT_VALUE     0
#define OV_CONTRAST_MIN              -100
#define OV_CONTRAST_MAX               100

#define OV_SATURATION_DEFAULT_VALUE   100
#define OV_SATURATION_MIN             0
#define OV_SATURATION_MAX             200

#define CLAMP_ATTR(a,max,min) (a>max?max:(a<min?min:a))

/*
 * OCMD - Overlay Command Register
 */
#define OCMD_REGISTER           0x30168
#define MIRROR_MODE             (0x3<<17)
#define MIRROR_HORIZONTAL       (0x1<<17)
#define MIRROR_VERTICAL         (0x2<<17)
#define MIRROR_BOTH             (0x3<<17)
#define OV_BYTE_ORDER           (0x3<<14)
#define UV_SWAP                 (0x1<<14)
#define Y_SWAP                  (0x2<<14)
#define Y_AND_UV_SWAP           (0x3<<14)
#define SOURCE_FORMAT           (0xf<<10)
#define RGB_888                 (0x1<<10)
#define RGB_555                 (0x2<<10)
#define RGB_565                 (0x3<<10)
#define NV12                    (0xb<<10)
#define YUV_422                 (0x8<<10)
#define YUV_411                 (0x9<<10)
#define YUV_420                 (0xc<<10)
#define YUV_422_PLANAR          (0xd<<10)
#define YUV_410                 (0xe<<10)
#define TVSYNC_FLIP_PARITY      (0x1<<9)
#define TVSYNC_FLIP_ENABLE      (0x1<<7)
#define BUF_TYPE                (0x1<<5)
#define BUF_TYPE_FRAME          (0x0<<5)
#define BUF_TYPE_FIELD          (0x1<<5)
#define TEST_MODE               (0x1<<4)
#define BUFFER_SELECT           (0x3<<2)
#define BUFFER0                 (0x0<<2)
#define BUFFER1                 (0x1<<2)
#define FIELD_SELECT            (0x1<<1)
#define FIELD0                  (0x0<<1)
#define FIELD1                  (0x1<<1)
#define OVERLAY_ENABLE          0x1

#define OFC_UPDATE              0x1

/*
* OVADD - Overlay Register Update Address Register
*/
#define OVADD_PIPE_A           (0x0<<6)
#define OVADD_PIPE_B           (0x2<<6)
#define OVADD_PIPE_C           (0x1<<6)
#define LOAD_IEP_BW_EXPANSION  (0x1<<4)
#define LOAD_IEP_BS_SCC                (0x1<<3)
#define LOAD_IEP_CSC           (0x1<<2)
#define LOAD_IEP_DEBUG         (0x1<<1)
#define LOAD_COEFFICEINT       (0x1<<0)

/* OCONFIG register */
#define CC_OUT_8BIT             (0x1<<3)
#define OVERLAY_PIPE_MASK       (0x1<<18)
#define OVERLAY_PIPE_A          (0x0<<18)
#define OVERLAY_PIPE_B          (0x1<<18)
#define IEP_LITE_BYPASS                (0x1<<27)
#define OVERLAY_C_PIPE_MASK      (0x3<<17)
#define OVERLAY_C_PIPE_A         (0x0<<17)
#define OVERLAY_C_PIPE_B         (0x2<<17)
#define OVERLAY_C_PIPE_C         (0x1<<17)
#define GAMMA2_ENBL             (0x1<<16)
#define ZORDER_TOP             (0x0<<15)
#define ZORDER_BOTTOM          (0x1<<15)
#define CSC_MODE_BT709          (0x1<<5)
#define CSC_MODE_BT601          (0x0<<5)
#define CSC_BYPASS             (0x1<<4)
#define THREE_LINE_BUFFERS      (0x1<<0)
#define TWO_LINE_BUFFERS        (0x0<<0)

/* DCLRKM register */
#define DEST_KEY_ENABLE         (0x1<<31)
#define CONST_ALPHA_ENABLE      (0x1<<30)

/* Polyphase filter coefficients */
#define N_HORIZ_Y_TAPS          5
#define N_VERT_Y_TAPS           3
#define N_HORIZ_UV_TAPS         3
#define N_VERT_UV_TAPS          3
#define N_PHASES                17
#define MAX_TAPS                5

/* Filter cutoff frequency limits. */
#define MIN_CUTOFF_FREQ         1.0
#define MAX_CUTOFF_FREQ         3.0

#define RGB16ToColorKey(c) \
(((c & 0xF800) << 8) | ((c & 0x07E0) << 5) | ((c & 0x001F) << 3))

#define RGB15ToColorKey(c) \
(((c & 0x7c00) << 9) | ((c & 0x03E0) << 6) | ((c & 0x001F) << 3))

typedef struct {
    uint32_t x1;
    uint32_t x2;
    uint32_t y1;
    uint32_t y2;
} BoxRec, *BoxPtr;

typedef struct {
    uint32_t OBUF_0Y;
    uint32_t OBUF_1Y;
    uint32_t OBUF_0U;
    uint32_t OBUF_0V;
    uint32_t OBUF_1U;
    uint32_t OBUF_1V;
    uint32_t OSTRIDE;
    uint32_t YRGB_VPH;
    uint32_t UV_VPH;
    uint32_t HORZ_PH;
    uint32_t INIT_PHS;
    uint32_t DWINPOS;
    uint32_t DWINSZ;
    uint32_t SWIDTH;
    uint32_t SWIDTHSW;
    uint32_t SHEIGHT;
    uint32_t YRGBSCALE;
    uint32_t UVSCALE;
    uint32_t OCLRC0;
    uint32_t OCLRC1;
    uint32_t DCLRKV;
    uint32_t DCLRKM;
    uint32_t SCHRKVH;
    uint32_t SCHRKVL;
    uint32_t SCHRKEN;
    uint32_t OCONFIG;
    uint32_t OCMD;
    uint32_t RESERVED1;                 /* 0x6C */
    uint32_t OSTART_0Y;                 /* for i965 */
    uint32_t OSTART_1Y;         /* for i965 */
    uint32_t OSTART_0U;
    uint32_t OSTART_0V;
    uint32_t OSTART_1U;
    uint32_t OSTART_1V;
    uint32_t OTILEOFF_0Y;
    uint32_t OTILEOFF_1Y;
    uint32_t OTILEOFF_0U;
    uint32_t OTILEOFF_0V;
    uint32_t OTILEOFF_1U;
    uint32_t OTILEOFF_1V;
    uint32_t FASTHSCALE;                        /* 0xA0 */
    uint32_t UVSCALEV;                  /* 0xA4 */

    uint32_t RESERVEDC[(0x200 - 0xA8) / 4];                /* 0xA8 - 0x1FC */
    uint16_t Y_VCOEFS[N_VERT_Y_TAPS * N_PHASES];                   /* 0x200 */
    uint16_t RESERVEDD[0x100 / 2 - N_VERT_Y_TAPS * N_PHASES];
    uint16_t Y_HCOEFS[N_HORIZ_Y_TAPS * N_PHASES];                  /* 0x300 */
    uint16_t RESERVEDE[0x200 / 2 - N_HORIZ_Y_TAPS * N_PHASES];
    uint16_t UV_VCOEFS[N_VERT_UV_TAPS * N_PHASES];                 /* 0x500 */
    uint16_t RESERVEDF[0x100 / 2 - N_VERT_UV_TAPS * N_PHASES];
    uint16_t UV_HCOEFS[N_HORIZ_UV_TAPS * N_PHASES];        /* 0x600 */
    uint16_t RESERVEDG[0xa00 / 2 - N_HORIZ_UV_TAPS * N_PHASES];
    uint32_t IEP_SPACE[(0x3401c - 0x31000)/4];
} I830OverlayRegRec, *I830OverlayRegPtr;


#define Degree (2*PI / 360.0)
#define PI 3.1415927


typedef struct {
    uint8_t sign;
    uint16_t mantissa;
    uint8_t exponent;
} coeffRec, *coeffPtr;

typedef struct _ov_psb_fixed32 {
    union {
        struct {
            unsigned short Fraction;
            short Value;
        };
        long ll;
    };
} ov_psb_fixed32;

typedef struct _PsbPortPrivRec {
    int curBuf;
    int is_mfld;
    int subpicture_enabled;
    unsigned int subpicture_enable_mask;
    int overlayA_enabled;
    int overlayC_enabled;

    /* used to check downscale*/
    short width_save;
    short height_save;

    /* information of display attribute */
    ov_psb_fixed32 brightness;
    ov_psb_fixed32 contrast;
    ov_psb_fixed32 saturation;
    ov_psb_fixed32 hue;
    
    void * p_iep_lite_context;
    
    /* hwoverlay */
    uint32_t gamma0;
    uint32_t gamma1;
    uint32_t gamma2;
    uint32_t gamma3;
    uint32_t gamma4;
    uint32_t gamma5;
    uint32_t colorKey;

    int oneLineMode;
    int scaleRatio;
    int rotation;

    struct _WsbmBufferObject *wsbo[2];
    uint32_t YBuf0offset;
    uint32_t UBuf0offset;
    uint32_t VBuf0offset;
    uint32_t YBuf1offset;
    uint32_t UBuf1offset;
    uint32_t VBuf1offset;
    unsigned char *regmap[2];
} PsbPortPrivRec, *PsbPortPrivPtr;


int psb_coverlay_init(VADriverContextP ctx);
int psb_coverlay_stop(VADriverContextP ctx);
int psb_coverlay_exit(VADriverContextP ctx);

VAStatus psb_putsurface_overlay(
    VADriverContextP ctx,
    VASurfaceID surface,    
    short srcx,
    short srcy,
    unsigned short srcw,
    unsigned short srch,
    short destx,
    short desty,    
    unsigned short destw,
    unsigned short desth,
    unsigned int flags, /* de-interlacing flags */
    int overlayId, 
    int pipeId
);

enum pipe_id_t{
   PIPEA = 0,
   PIPEB,
   PIPEC,
};

enum overlay_id_t{
   OVERLAY_A = 0,
   OVERLAY_C,
};

#endif /* _PSB_OVERLAY_H_ */
