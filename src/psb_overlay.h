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

#ifndef _PSB_OVERLAY_H_
#define _PSB_OVERLAY_H_


#define USE_OVERLAY 1
#define USE_DISPLAY_C_SPRITE 0
#define USE_PVR2D 1
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
#define USE_FIT_SCR_SIZE 1
#define USE_CLIP_FUNC 0
#define USE_SCALE_FUNC 1
#define USE_ROTATION_FUNC 1


#if 0
#define Bool unsigned int
#define TRUE 1
#define FALSE 0
#endif
#define BadAlloc -1
#define Success 0

/* FIXME this will be removed later after using pvr2d */
#if 1
#define Rotation unsigned int
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

/* OCONFIG register */
#define CC_OUT_8BIT             (0x1<<3)
#define OVERLAY_PIPE_MASK       (0x1<<18)
#define OVERLAY_PIPE_A          (0x0<<18)
#define OVERLAY_PIPE_B          (0x1<<18)
#define GAMMA2_ENBL             (0x1<<16)
#define CSC_MODE_BT709          (0x1<<5)
#define CSC_MODE_BT601          (0x0<<5)
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
    uint16_t RESERVEDG[0x100 / 2 - N_HORIZ_UV_TAPS * N_PHASES];
} I830OverlayRegRec, *I830OverlayRegPtr;

typedef enum _psb_nominalrange
{
    PSB_NominalRangeMask = 0x07,
    PSB_NominalRange_Unknown = 0,
    PSB_NominalRange_Normal = 1,
    PSB_NominalRange_Wide = 2,
    /* explicit range forms */
    PSB_NominalRange_0_255 = 1,
    PSB_NominalRange_16_235 = 2,
    PSB_NominalRange_48_208 = 3
} psb_nominalrange;

typedef enum _psb_videotransfermatrix
{
    PSB_VideoTransferMatrixMask = 0x07,
    PSB_VideoTransferMatrix_Unknown = 0,
    PSB_VideoTransferMatrix_BT709 = 1,
    PSB_VideoTransferMatrix_BT601 = 2,
    PSB_VideoTransferMatrix_SMPTE240M = 3
} psb_videotransfermatrix;

typedef struct _ov_psb_fixed32
{
    union
    {
        struct
        {
            unsigned short Fraction;
            short Value;
        };
        long ll;
    };
} ov_psb_fixed32;

#define Degree (2*PI / 360.0)
#define PI 3.1415927

typedef struct _psb_transform_coeffs_
{
    double rY, rCb, rCr;
    double gY, gCb, gCr;
    double bY, bCb, bCr;
} psb_transform_coeffs;

typedef struct {
    uint8_t sign;
    uint16_t mantissa;
    uint8_t exponent;
} coeffRec, *coeffPtr;

typedef struct _psb_coeffs_
{
    signed char rY;
    signed char rU;
    signed char rV;
    signed char gY;
    signed char gU;
    signed char gV;
    signed char bY;
    signed char bU;
    signed char bV;
    unsigned char rShift;
    unsigned char gShift;
    unsigned char bShift;
    signed short rConst;
    signed short gConst;
    signed short bConst;
} psb_coeffs_s, *psb_coeffs_p;

int psbInitVideo(VADriverContextP ctx);
int psbDeInitVideo(VADriverContextP ctx);
int I830PutImage(VADriverContextP ctx,
             VASurfaceID surface,
             short src_x, short src_y,
             short src_w, short src_h,
             short drw_x, short drw_y,
             short drw_w, short drw_h, int id);
void I830StopVideo(VADriverContextP ctx);

VAStatus psb_putsurface_overlay(
    VADriverContextP ctx,
    VASurfaceID surface,
    Drawable draw,
    short srcx,
    short srcy,
    unsigned short srcw,
    unsigned short srch,
    short destx,
    short desty,
    unsigned short destw,
    unsigned short desth,
    VARectangle *cliprects, /* client supplied clip list */
    unsigned int number_cliprects, /* number of clip rects in the clip list */
    unsigned int flags /* de-interlacing flags */
);

#endif /* _PSB_OVERLAY_H_ */
