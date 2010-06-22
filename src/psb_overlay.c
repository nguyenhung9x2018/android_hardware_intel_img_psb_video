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

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <va/va_backend.h>
#include <wsbm/wsbm_manager.h>
#include <psb_drm.h>
#include "psb_drv_video.h"
#include "psb_output.h"
#include "psb_overlay.h"
#include "psb_xvva.h"

#define INIT_DRIVER_DATA psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;
#define SURFACE(id)	((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))

typedef struct _PsbPortPrivRec {
    int curBuf;
    Bool is_mfld;

    /* used to check downscale*/
    short width_save;
    short height_save;

    /* information of display attribute */
    ov_psb_fixed32 brightness;
    ov_psb_fixed32 contrast;
    ov_psb_fixed32 saturation;
    ov_psb_fixed32 hue;

    /* hwoverlay */
    uint32_t gamma0;
    uint32_t gamma1;
    uint32_t gamma2;
    uint32_t gamma3;
    uint32_t gamma4;
    uint32_t gamma5;
    uint32_t colorKey;

    //TODO zhaohan
    int oneLineMode;
    int scaleRatio;
    int rotation;

    struct _WsbmBufferObject *wsbo;
    uint32_t YBuf0offset;
    uint32_t UBuf0offset;
    uint32_t VBuf0offset;
    uint32_t YBuf1offset;
    uint32_t UBuf1offset;
    uint32_t VBuf1offset;
    unsigned char *regmap;
} PsbPortPrivRec, *PsbPortPrivPtr;


/**********************************************************************************************
 * I830ResetVideo
 *
 * Description: Use this function to reset the overlay register back buffer to its default
 * values.  Note that this function does not actually apply these values.  To do so, please
 * write to OVADD.
 **********************************************************************************************/
static void
I830ResetVideo(PsbPortPrivPtr pPriv)
{
    I830OverlayRegPtr overlay = (I830OverlayRegPtr)(pPriv->regmap);

    memset(overlay, 0, sizeof(*overlay));

    overlay->OCLRC0 = (pPriv->contrast.Value << 18) | (pPriv->brightness.Value & 0xff);
    overlay->OCLRC1 = pPriv->saturation.Value;

#if USE_DCLRK
    /* case bit depth 16 */
    overlay->DCLRKV = RGB16ToColorKey(pPriv->colorKey);
    overlay->DCLRKM |= DEST_KEY_ENABLE;
    overlay->DCLRKM &= ~CONST_ALPHA_ENABLE;
#else
    overlay->DCLRKM &= ~DEST_KEY_ENABLE;
#endif
    overlay->DWINSZ = 0x00000000;
    overlay->OCONFIG = CC_OUT_8BIT;

    if(pPriv->is_mfld) {
        overlay->OCONFIG &= OVERLAY_C_PIPE_A | (~OVERLAY_C_PIPE_MASK);
        overlay->OCONFIG |= IEP_LITE_BYPASS;    /* By pass IEP functionality */
        overlay->OCONFIG |= ZORDER_TOP;
    }
    else
        overlay->OCONFIG |= OVERLAY_PIPE_A; /* mrst */
}

static uint32_t I830BoundGammaElt (uint32_t elt, uint32_t eltPrev)
{
    elt &= 0xff;
    eltPrev &= 0xff;
    if (elt < eltPrev)
        elt = eltPrev;
    else if ((elt - eltPrev) > 0x7e)
        elt = eltPrev + 0x7e;
    return elt;
}

static uint32_t I830BoundGamma (uint32_t gamma, uint32_t gammaPrev)
{
    return (I830BoundGammaElt (gamma >> 24, gammaPrev >> 24) << 24 |
            I830BoundGammaElt (gamma >> 16, gammaPrev >> 16) << 16 |
            I830BoundGammaElt (gamma >>  8, gammaPrev >>  8) <<  8 |
            I830BoundGammaElt (gamma      , gammaPrev      ));
}

static void
I830UpdateGamma(VADriverContextP ctx, PsbPortPrivPtr pPriv)
{
    INIT_DRIVER_DATA;
    uint32_t gamma0 = pPriv->gamma0;
    uint32_t gamma1 = pPriv->gamma1;
    uint32_t gamma2 = pPriv->gamma2;
    uint32_t gamma3 = pPriv->gamma3;
    uint32_t gamma4 = pPriv->gamma4;
    uint32_t gamma5 = pPriv->gamma5;
    struct drm_psb_register_rw_arg regs;

    gamma1 = I830BoundGamma (gamma1, gamma0);
    gamma2 = I830BoundGamma (gamma2, gamma1);
    gamma3 = I830BoundGamma (gamma3, gamma2);
    gamma4 = I830BoundGamma (gamma4, gamma3);
    gamma5 = I830BoundGamma (gamma5, gamma4);

    memset(&regs, 0, sizeof(regs));
    regs.overlay_write_mask |= OV_REGRWBITS_OGAM_ALL;
    regs.overlay.OGAMC0 = gamma0;
    regs.overlay.OGAMC1 = gamma1;
    regs.overlay.OGAMC2 = gamma2;
    regs.overlay.OGAMC3 = gamma3;
    regs.overlay.OGAMC4 = gamma4;
    regs.overlay.OGAMC5 = gamma5;
    drmCommandWriteRead(driver_data->drm_fd, DRM_PSB_REGISTER_RW, &regs, sizeof(regs));
}

static void I830StopVideo(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    PsbPortPrivPtr pPriv = (PsbPortPrivPtr)(driver_data->dri_priv);
    long offset = wsbmBOOffsetHint(pPriv->wsbo) & 0x0FFFFFFF;
    struct drm_psb_register_rw_arg regs;

#if 0
    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
#endif

    I830ResetVideo(pPriv);

    memset(&regs, 0, sizeof(regs));
    regs.overlay_write_mask = OV_REGRWBITS_OVADD;
    regs.overlay.OVADD = offset;
    drmCommandWriteRead(driver_data->drm_fd, DRM_PSB_REGISTER_RW, &regs, sizeof(regs));
}

static int
i830_swidth (unsigned int offset, unsigned int width, unsigned int mask, int shift)
{
    int swidth = ((offset + width + mask) >> shift) - (offset >> shift);
    swidth <<= 1;
    swidth -= 1;
    return swidth << 2;
}

static Bool
SetCoeffRegs(double *coeff, int mantSize, coeffPtr pCoeff, int pos)
{
    int maxVal, icoeff, res;
    int sign;
    double c;

    sign = 0;
    maxVal = 1 << mantSize;
    c = *coeff;
    if (c < 0.0) {
        sign = 1;
        c = -c;
    }

    res = 12 - mantSize;
    if ((icoeff = (int)(c * 4 * maxVal + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 3;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(4 * maxVal);
    } else if ((icoeff = (int)(c * 2 * maxVal + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 2;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(2 * maxVal);
    } else if ((icoeff = (int)(c * maxVal + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 1;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(maxVal);
    } else if ((icoeff = (int)(c * maxVal * 0.5 + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 0;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(maxVal / 2);
    } else {
        /* Coeff out of range */
        return FALSE;
    }

    pCoeff[pos].sign = sign;
    if (sign)
        *coeff = -(*coeff);
    return TRUE;
}

static void
UpdateCoeff(int taps, double fCutoff, Bool isHoriz, Bool isY, coeffPtr pCoeff)
{
    int i, j, j1, num, pos, mantSize;
    double pi = 3.1415926535, val, sinc, window, sum;
    double rawCoeff[MAX_TAPS * 32], coeffs[N_PHASES][MAX_TAPS];
    double diff;
    int tapAdjust[MAX_TAPS], tap2Fix;
    Bool isVertAndUV;

    if (isHoriz)
        mantSize = 7;
    else
        mantSize = 6;

    isVertAndUV = !isHoriz && !isY;
    num = taps * 16;
    for (i = 0; i < num  * 2; i++) {
        val = (1.0 / fCutoff) * taps * pi * (i - num) / (2 * num);
        if (val == 0.0)
            sinc = 1.0;
        else
            sinc = sin(val) / val;

        /* Hamming window */
        window = (0.5 - 0.5 * cos(i * pi / num));
        rawCoeff[i] = sinc * window;
    }

    for (i = 0; i < N_PHASES; i++) {
        /* Normalise the coefficients. */
        sum = 0.0;
        for (j = 0; j < taps; j++) {
            pos = i + j * 32;
            sum += rawCoeff[pos];
        }
        for (j = 0; j < taps; j++) {
            pos = i + j * 32;
            coeffs[i][j] = rawCoeff[pos] / sum;
        }

        /* Set the register values. */
        for (j = 0; j < taps; j++) {
            pos = j + i * taps;
            if ((j == (taps - 1) / 2) && !isVertAndUV)
                SetCoeffRegs(&coeffs[i][j], mantSize + 2, pCoeff, pos);
            else
                SetCoeffRegs(&coeffs[i][j], mantSize, pCoeff, pos);
        }

        tapAdjust[0] = (taps - 1) / 2;
        for (j = 1, j1 = 1; j <= tapAdjust[0]; j++, j1++) {
            tapAdjust[j1] = tapAdjust[0] - j;
            tapAdjust[++j1] = tapAdjust[0] + j;
        }

        /* Adjust the coefficients. */
        sum = 0.0;
        for (j = 0; j < taps; j++)
            sum += coeffs[i][j];
        if (sum != 1.0) {
            for (j1 = 0; j1 < taps; j1++) {
                tap2Fix = tapAdjust[j1];
                diff = 1.0 - sum;
                coeffs[i][tap2Fix] += diff;
                pos = tap2Fix + i * taps;
                if ((tap2Fix == (taps - 1) / 2) && !isVertAndUV)
                    SetCoeffRegs(&coeffs[i][tap2Fix], mantSize + 2, pCoeff, pos);
                else
                    SetCoeffRegs(&coeffs[i][tap2Fix], mantSize, pCoeff, pos);

                sum = 0.0;
                for (j = 0; j < taps; j++)
                    sum += coeffs[i][j];
                if (sum == 1.0)
                    break;
            }
        }
    }
}

static void
i830_display_video(
    VADriverContextP ctx, PsbPortPrivPtr pPriv, VASurfaceID surface,
    int id, short width, short height,
    int dstPitch, int srcPitch, int x1, int y1, int x2, int y2, BoxPtr dstBox,
    short src_w, short src_h, short drw_w, short drw_h, unsigned int flags)
{
    INIT_DRIVER_DATA;
    unsigned int        swidth, swidthy, swidthuv;
    unsigned int        mask, shift, offsety, offsetu;
    int                 tmp;
    uint32_t            OCMD;
    Bool                scaleChanged = FALSE;

    unsigned int offset = wsbmBOOffsetHint(pPriv->wsbo) & 0x0FFFFFFF;

    I830OverlayRegPtr overlay = (I830OverlayRegPtr)(pPriv->regmap);
    struct drm_psb_register_rw_arg regs;

    /* FIXME: don't know who and why add this 
     *        comment it for full screen scale issue
     *        any concern contact qiang.miao@intel.com 
     */
#if 0
    if(drw_w >= 800) {
        x2 = x2/4;
        y2 = y2/4;
        dstBox->x2 = dstBox->x2/4;
        dstBox->y2 = dstBox->y2/4;
        drw_w = drw_w /4;
        drw_h = drw_h /4;
    }
#endif
    //FIXME(Ben):There is a hardware bug which prevents overlay from
    //           being reenabled after being disabled.  Until this is
    //           fixed, don't disable the overlay.  We just make it
    //           fully transparent and set it's window size to zero.
    //           once hardware is fixed, remove this line disabling
    //           CONST_ALPHA_ENABLE.
    //    if(IS_MRST(pDevice))
    overlay->DCLRKM &= ~CONST_ALPHA_ENABLE;
    overlay->DCLRKM |= DEST_KEY_ENABLE;
    overlay->DCLRKV = RGB16ToColorKey(pPriv->colorKey);

#if USE_ROTATION_FUNC
    switch (pPriv->rotation) {
    case RR_Rotate_0:
        break;
    case RR_Rotate_90:
        tmp = dstBox->x1;
        dstBox->x1 = dstBox->y1;
        dstBox->y1 = pPriv->height_save - tmp;
        tmp = dstBox->x2;
        dstBox->x2 = dstBox->y2;
        dstBox->y2 = pPriv->height_save - tmp;
        tmp = dstBox->y1;
        dstBox->y1 = dstBox->y2;
        dstBox->y2 = tmp;
        break;
    case RR_Rotate_180:
        tmp = dstBox->x1;
        dstBox->x1 = pPriv->width_save - dstBox->x2;
        dstBox->x2 = pPriv->width_save - tmp;
        tmp = dstBox->y1;
        dstBox->y1 = pPriv->height_save - dstBox->y2;
        dstBox->y2 = pPriv->height_save - tmp;
        break;
    case RR_Rotate_270:
        tmp = dstBox->x1;
        dstBox->x1 = pPriv->width_save - dstBox->y1;
        dstBox->y1 = tmp;
        tmp = dstBox->x2;
        dstBox->x2 = pPriv->width_save - dstBox->y2;
        dstBox->y2 = tmp;
        tmp = dstBox->x1;
        dstBox->x1 = dstBox->x2;
        dstBox->x2 = tmp;
        break;
    }

    if (pPriv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
        tmp = width;
        width = height;
        height = tmp;
        tmp = drw_w;
        drw_w = drw_h;
        drw_h = tmp;
        tmp = src_w;
        src_w = src_h;
        src_h = tmp;
    }
#endif

    if (pPriv->oneLineMode) {
        /* change the coordinates with panel fitting active */
        dstBox->y1 = (((dstBox->y1 - 1) * pPriv->scaleRatio) >> 16) + 1;
        dstBox->y2 = ((dstBox->y2 * pPriv->scaleRatio) >> 16) + 1;

        /* Now, alter the height, so we scale to the correct size */
        drw_h = ((drw_h * pPriv->scaleRatio) >> 16) + 1;
    }

    shift = 6;
    mask = 0x3f;

    if (pPriv->curBuf == 0) {
        offsety = pPriv->YBuf0offset;
        offsetu = pPriv->UBuf0offset;
    } else {
        offsety = pPriv->YBuf1offset;
        offsetu = pPriv->UBuf1offset;
    }

    switch (id) {
    case VA_FOURCC_NV12:
        overlay->SWIDTH = width | ((width/2 & 0x7ff) << 16);
        swidthy = i830_swidth (offsety, width, mask, shift);
        swidthuv = i830_swidth (offsetu, width/2, mask, shift);
        overlay->SWIDTHSW = (swidthy) | (swidthuv << 16);
        overlay->SHEIGHT = height | ((height / 2) << 16);
        break;
    case VA_FOURCC_YV12:
    case VA_FOURCC_I420:
        overlay->SWIDTH = width | ((width/2 & 0x7ff) << 16);
        swidthy  = i830_swidth (offsety, width, mask, shift);
        swidthuv = i830_swidth (offsetu, width/2, mask, shift);
        overlay->SWIDTHSW = (swidthy) | (swidthuv << 16);
        overlay->SHEIGHT = height | ((height / 2) << 16);
        break;
    case VA_FOURCC_UYVY:
    case VA_FOURCC_YUY2:
    default:
        overlay->SWIDTH = width;
        swidth = ((offsety + (width << 1) + mask) >> shift) -
            (offsety >> shift);

        swidth <<= 1;
        swidth -= 1;
        swidth <<= 2;

        overlay->SWIDTHSW = swidth;
        overlay->SHEIGHT = height;
        break;
    }

    overlay->DWINPOS = (dstBox->y1 << 16) | dstBox->x1;

    overlay->DWINSZ = (((dstBox->y2 - dstBox->y1) << 16) |
                       (dstBox->x2 - dstBox->x1));

    /* buffer locations */
    overlay->OBUF_0Y = pPriv->YBuf0offset;
    overlay->OBUF_0U = pPriv->UBuf0offset;
    overlay->OBUF_0V = pPriv->VBuf0offset;
    overlay->OBUF_1Y = pPriv->YBuf1offset;
    overlay->OBUF_1U = pPriv->UBuf1offset;
    overlay->OBUF_1V = pPriv->VBuf1offset;

    /*
     * Calculate horizontal and vertical scaling factors and polyphase
     * coefficients.
     */

    if (1) {
        int xscaleInt, xscaleFract, yscaleInt, yscaleFract;
        int xscaleIntUV, xscaleFractUV;
        int yscaleIntUV, yscaleFractUV;
        /* UV is half the size of Y -- YUV420 */
        int uvratio = 2;
        uint32_t newval;
        coeffRec xcoeffY[N_HORIZ_Y_TAPS * N_PHASES];
        coeffRec xcoeffUV[N_HORIZ_UV_TAPS * N_PHASES];
        int i, j, pos;
        int deinterlace_factor;

        /*
         * Y down-scale factor as a multiple of 4096.
         */
        if ((id == VA_FOURCC_NV12) && (0 != (flags & (VA_TOP_FIELD | VA_BOTTOM_FIELD ))))
            deinterlace_factor = 2;
        else
            deinterlace_factor = 1;

        /* deinterlace requires twice of VSCALE setting*/
        if (src_w == drw_w && src_h == drw_h)
        {
            xscaleFract = 1<<12;
            yscaleFract = (1<<12) / deinterlace_factor;
        }
        else
        {
            xscaleFract = ((src_w - 1) << 12) / drw_w;
            yscaleFract = ((src_h - 1) << 12) / (deinterlace_factor * drw_h);
        }

        /* Calculate the UV scaling factor. */
        xscaleFractUV = xscaleFract / uvratio;
        yscaleFractUV = yscaleFract / uvratio;

        /*
         * To keep the relative Y and UV ratios exact, round the Y scales
         * to a multiple of the Y/UV ratio.
         */
        xscaleFract = xscaleFractUV * uvratio;
        yscaleFract = yscaleFractUV * uvratio;

        /* Integer (un-multiplied) values. */
        xscaleInt = xscaleFract >> 12;
        yscaleInt = yscaleFract >> 12;

        xscaleIntUV = xscaleFractUV >> 12;
        yscaleIntUV = yscaleFractUV >> 12;

        /* shouldn't get here */
        if (xscaleInt > 7) {
            return;
        }

        /* shouldn't get here */
        if (xscaleIntUV > 7) {
            return;
        }

        if(pPriv->is_mfld)
            newval = (xscaleInt << 15) |
                ((xscaleFract & 0xFFF) << 3) | ((yscaleFract & 0xFFF) << 20);
        else
            newval = (xscaleInt << 16) |
                ((xscaleFract & 0xFFF) << 3) | ((yscaleFract & 0xFFF) << 20);

        if (newval != overlay->YRGBSCALE) {
            scaleChanged = TRUE;
            overlay->YRGBSCALE = newval;
        }

        if(pPriv->is_mfld)
            newval = (xscaleIntUV << 15) | ((xscaleFractUV & 0xFFF) << 3) |
                ((yscaleFractUV & 0xFFF) << 20);
        else
            newval = (xscaleIntUV << 16) | ((xscaleFractUV & 0xFFF) << 3) |
                ((yscaleFractUV & 0xFFF) << 20);

        if (newval != overlay->UVSCALE) {
            scaleChanged = TRUE;
            overlay->UVSCALE = newval;
        }

        newval = yscaleInt << 16 | yscaleIntUV;
        if (newval != overlay->UVSCALEV) {
            scaleChanged = TRUE;
            overlay->UVSCALEV = newval;
        }

        /* Recalculate coefficients if the scaling changed. */

        /*
         * Only Horizontal coefficients so far.
         */
        if (scaleChanged) {
            double fCutoffY;
            double fCutoffUV;

            fCutoffY = xscaleFract / 4096.0;
            fCutoffUV = xscaleFractUV / 4096.0;

            /* Limit to between 1.0 and 3.0. */
            if (fCutoffY < MIN_CUTOFF_FREQ)
                fCutoffY = MIN_CUTOFF_FREQ;
            if (fCutoffY > MAX_CUTOFF_FREQ)
                fCutoffY = MAX_CUTOFF_FREQ;
            if (fCutoffUV < MIN_CUTOFF_FREQ)
                fCutoffUV = MIN_CUTOFF_FREQ;
            if (fCutoffUV > MAX_CUTOFF_FREQ)
                fCutoffUV = MAX_CUTOFF_FREQ;

            UpdateCoeff(N_HORIZ_Y_TAPS, fCutoffY, TRUE, TRUE, xcoeffY);
            UpdateCoeff(N_HORIZ_UV_TAPS, fCutoffUV, TRUE, FALSE, xcoeffUV);

            for (i = 0; i < N_PHASES; i++) {
                for (j = 0; j < N_HORIZ_Y_TAPS; j++) {
                    pos = i * N_HORIZ_Y_TAPS + j;
                    overlay->Y_HCOEFS[pos] = (xcoeffY[pos].sign << 15 |
                                              xcoeffY[pos].exponent << 12 |
                                              xcoeffY[pos].mantissa);
                }
            }
            for (i = 0; i < N_PHASES; i++) {
                for (j = 0; j < N_HORIZ_UV_TAPS; j++) {
                    pos = i * N_HORIZ_UV_TAPS + j;
                    overlay->UV_HCOEFS[pos] = (xcoeffUV[pos].sign << 15 |
                                               xcoeffUV[pos].exponent << 12 |
                                               xcoeffUV[pos].mantissa);
                }
            }
        }
    }

    OCMD = OVERLAY_ENABLE;

    switch (id) {
    case VA_FOURCC_NV12:
        overlay->OSTRIDE = dstPitch | (dstPitch << 16);
        OCMD &= ~SOURCE_FORMAT;
        OCMD &= ~OV_BYTE_ORDER;
        OCMD |= NV12;//in the spec, there are two NV12, which to use?
        break;
    case VA_FOURCC_YV12:
    case VA_FOURCC_I420:
        /* set UV vertical phase to -0.25 */
        /* overlay->UV_VPH = 0x30003000; */
        overlay->OSTRIDE = (dstPitch * 2) | (dstPitch << 16);
        OCMD &= ~SOURCE_FORMAT;
        OCMD &= ~OV_BYTE_ORDER;
        OCMD |= YUV_420;
        break;
    case VA_FOURCC_UYVY:
    case VA_FOURCC_YUY2:
        overlay->OSTRIDE = dstPitch;
        OCMD &= ~SOURCE_FORMAT;
        OCMD |= YUV_422;
        OCMD &= ~OV_BYTE_ORDER;
        if (id == VA_FOURCC_UYVY)
            OCMD |= Y_SWAP;
        break;
    }

    if (flags & (VA_TOP_FIELD | VA_BOTTOM_FIELD )) {
	OCMD |= BUF_TYPE_FIELD;
	OCMD &= ~FIELD_SELECT;

	if (flags & VA_BOTTOM_FIELD) {
	    OCMD |= FIELD1;
	    overlay->OBUF_0Y = pPriv->YBuf0offset - srcPitch;
	    overlay->OBUF_0U = pPriv->UBuf0offset - srcPitch;
	    overlay->OBUF_0V = pPriv->VBuf0offset - srcPitch;
	    overlay->OBUF_1Y = pPriv->YBuf1offset - srcPitch;
	    overlay->OBUF_1U = pPriv->UBuf1offset - srcPitch;
	    overlay->OBUF_1V = pPriv->VBuf1offset - srcPitch;
	}
	else
	    OCMD |= FIELD0;
    } else {
	OCMD &= ~(FIELD_SELECT);
	OCMD &= ~BUF_TYPE_FIELD;
    }

    OCMD &= ~(BUFFER_SELECT);

    if (pPriv->curBuf == 0)
        OCMD |= BUFFER0;
    else
        OCMD |= BUFFER1;

    overlay->OCMD = OCMD;

    memset(&regs, 0, sizeof(regs));
    regs.overlay_write_mask = OV_REGRWBITS_OVADD;
    regs.overlay.OVADD = offset | 1;
    drmCommandWriteRead(driver_data->drm_fd, DRM_PSB_REGISTER_RW, &regs, sizeof(regs));
}

/*
 * The source rectangle of the video is defined by (src_x, src_y, src_w, src_h).
 * The dest rectangle of the video is defined by (drw_x, drw_y, drw_w, drw_h).
 * id is a fourcc code for the format of the video.
 * buf is the pointer to the source data in system memory.
 * width and height are the w/h of the source data.
 * If "sync" is TRUE, then we must be finished with *buf at the point of return
 * (which we always are).
 * clipBoxes is the clipping region in screen space.
 * data is a pointer to our port private.
 * pDraw is a Drawable, which might not be the screen in the case of
 * compositing.  It's a new argument to the function in the 1.1 server.
 */
static int I830PutImage(
    VADriverContextP ctx,
    VASurfaceID surface,
    short src_x, short src_y,
    short src_w, short src_h,
    short drw_x, short drw_y,
    short drw_w, short drw_h,
    int fourcc, int flags)
{
    INIT_DRIVER_DATA;
    int x1, x2, y1, y2;
    int width, height;
    int top, left, npixels;
    int pitch = 0, pitch2 = 0;
    unsigned int pre_add;
    unsigned int gtt_ofs;
    struct _WsbmBufferObject *drm_buf;
    BoxRec dstBox;
    PsbPortPrivPtr pPriv;

    pPriv = (PsbPortPrivPtr)(driver_data->dri_priv);

    object_surface_p obj_surface = SURFACE(surface);
    psb_surface_p psb_surface = obj_surface->psb_surface;

    switch (fourcc) {
    case VA_FOURCC_NV12:
        width = obj_surface->width;
        height = obj_surface->height;
        break;
    default:
        width = obj_surface->width;
        height = obj_surface->height;
        break;
    }

    /* If dst width and height are less than 1/8th the src size, the
     * src/dst scale factor becomes larger than 8 and doesn't fit in
     * the scale register.
     */
    if(src_w >= (drw_w * 8))
        drw_w = src_w/7;

    if(src_h >= (drw_h * 8))
        drw_h = src_h/7;

    /* Clip */
    x1 = src_x;
    x2 = src_x + src_w;
    y1 = src_y;
    y2 = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

#if USE_CLIP_FUNC
    if (!i830_get_crtc(pScrn, &crtc, &dstBox))
        return Success;

    /*
     *Update drw_* and 'clipBoxes' according to current downscale/upscale state
     * Make sure the area determined by drw_* is in 'clipBoxes'
     */
    if (crtc->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
        h_ratio = (float)pScrn->pScreen->height / pPriv->width_save;
        v_ratio = (float)pScrn->pScreen->width / pPriv->height_save;
    } else {
        h_ratio = (float)pScrn->pScreen->width / pPriv->width_save;
        v_ratio = (float)pScrn->pScreen->height / pPriv->height_save;
    }

    /* Horizontal downscale/upscale */
    if ((int)h_ratio)
        clipBoxes->extents.x1 /= h_ratio;
    else if (!(int)h_ratio)
        clipBoxes->extents.x2 /= h_ratio;

    /* Vertical downscale/upscale */
    if ((int)v_ratio)
        clipBoxes->extents.y1 /= v_ratio;
    else if (!(int)v_ratio)
        clipBoxes->extents.y2 /= v_ratio;

    drw_x /= h_ratio;
    drw_y /= v_ratio;
    drw_w /= h_ratio;
    drw_h /= v_ratio;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

    /* Count in client supplied clipboxes */
    clipRegion = clipBoxes;
    psb_perform_clip(pScrn, vaPtr->clipbox, vaPtr->num_clipbox, clipBoxes, clipRegion, pDraw);

    if (!i830_clip_video_helper(pScrn,
                                &crtc,
                                &dstBox, &x1, &x2, &y1, &y2, clipRegion,
                                width, height)) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "%s: Fail to clip video to any crtc!\n", __FUNCTION__);
        return 0;
    }
#endif

    switch (fourcc) {
    case VA_FOURCC_NV12:
        pitch = (width + 0x3) & ~0x3;
	pitch2 = psb_surface->stride;
        break;
    case VA_FOURCC_YV12:
    case VA_FOURCC_I420:
        pitch = (width + 0x3) & ~0x3;
        break;
#if USE_DISPLAY_C_SPRITE
    case FOURCC_RGBA:
        pitch = width << 2;
        break;
#endif
    case VA_FOURCC_UYVY:
    case VA_FOURCC_YUY2:
    default:
        pitch = width << 1;
        break;
    }

    top = y1 >> 16;
    left = (x1 >> 16) & ~1;
    npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;

    if (fourcc == VA_FOURCC_NV12) {
	pre_add = psb_surface->buf.buffer_ofs;
	drm_buf = psb_surface->buf.drm_buf;
	gtt_ofs = wsbmBOOffsetHint(drm_buf) & 0x0FFFFFFF;

	pPriv->YBuf0offset = pre_add + gtt_ofs  + top * pitch + left;
	pPriv->YBuf1offset = pPriv->YBuf0offset;
	pPriv->UBuf0offset = pre_add + gtt_ofs + (pitch2  * height) + top * (pitch2/2) + left;
	pPriv->VBuf0offset = pPriv->UBuf0offset;
	pPriv->UBuf1offset = pPriv->UBuf0offset;
	pPriv->VBuf1offset = pPriv->UBuf0offset;
    } else {
        //TBD
        //pPriv->YBuf0offset = pPriv->videoBuf0_gtt_offset << PAGE_SHIFT;
        //pPriv->YBuf1offset = pPriv->videoBuf1_gtt_offset << PAGE_SHIFT;
        if (pPriv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
            pPriv->UBuf0offset = pPriv->YBuf0offset + (pitch2  * width);
            pPriv->VBuf0offset = pPriv->UBuf0offset + (pitch2 * width / 2);
            pPriv->UBuf1offset = pPriv->YBuf1offset + (pitch2 * width);
            pPriv->VBuf1offset = pPriv->UBuf1offset + (pitch2 * width / 2);
        } else {
            pPriv->UBuf0offset = pPriv->YBuf0offset + (pitch2 * height);
            pPriv->VBuf0offset = pPriv->UBuf0offset + (pitch2 * height / 2);
            pPriv->UBuf1offset = pPriv->YBuf1offset + (pitch2 * height);
            pPriv->VBuf1offset = pPriv->UBuf1offset + (pitch2 * height / 2);
        }
    }

#if USE_DISPLAY_C_SPRITE
    if (fourcc == FOURCC_RGBA   \
        || (fourcc == FOURCC_XVVA   \
            && (pPriv->rotation != RR_Rotate_0) \
            && (vaPtr->dst_srf.fourcc == VA_FOURCC_RGBA)))
        i830_display_video_sprite(pScrn, crtc, width, height, dstPitch,
                                  &dstBox, sprite_offset);
    else
#endif
        i830_display_video(ctx, pPriv, surface, fourcc, width, height, pitch2, pitch,
                           x1, y1, x2, y2, &dstBox, src_w, src_h,
                           drw_w, drw_h, flags);

    // FIXME : do I use two buffers here really?
    //    pPriv->curBuf = (pPriv->curBuf + 1) & 1;

    return Success;
}



static PsbPortPrivPtr
psbPortPrivCreate(void)
{
    PsbPortPrivPtr pPriv;

    pPriv = calloc(1, sizeof(*pPriv));
    if (!pPriv)
        return NULL;

#if 0
    REGION_NULL(pScreen, &pPriv->clip);
#endif

    /* coeffs defaut value */
    pPriv->brightness.Value = OV_BRIGHTNESS_DEFAULT_VALUE;
    pPriv->brightness.Fraction = 0;

    pPriv->contrast.Value = OV_CONTRAST_DEFAULT_VALUE;
    pPriv->contrast.Fraction = 0;

    pPriv->hue.Value = OV_HUE_DEFAULT_VALUE;
    pPriv->hue.Fraction = 0;

    pPriv->saturation.Value = OV_SATURATION_DEFAULT_VALUE;
    pPriv->saturation.Fraction = 0;

    /* FIXME: is this right? set up to current screen size */
#if 1
    pPriv->width_save = 1024;
    pPriv->height_save = 600;
#endif

    return pPriv;
}

static void
psbPortPrivDestroy(VADriverContextP ctx, PsbPortPrivPtr pPriv)
{
    if (!pPriv)
        return;

    I830StopVideo(ctx);

    wsbmBOUnmap(pPriv->wsbo);
    wsbmBOUnreference(&pPriv->wsbo);

    free(pPriv);
}

static PsbPortPrivPtr
psbSetupImageVideoOverlay(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    PsbPortPrivPtr pPriv;
    int ret;

    pPriv = psbPortPrivCreate();
    if (!pPriv)
        goto out_err;

    pPriv->is_mfld = IS_MFLD(driver_data);
    /* use green as color key by default for android media player */
    pPriv->colorKey = 0 /*0x0440*/;

    pPriv->brightness.Value = -19; /* (255/219) * -16 */
    pPriv->contrast.Value = 75;  /* 255/219 * 64 */
    pPriv->saturation.Value = 146; /* 128/112 * 128 */
    pPriv->gamma5 = 0xc0c0c0;
    pPriv->gamma4 = 0x808080;
    pPriv->gamma3 = 0x404040;
    pPriv->gamma2 = 0x202020;
    pPriv->gamma1 = 0x101010;
    pPriv->gamma0 = 0x080808;

    pPriv->rotation = RR_Rotate_0;

#if 0
    /* gotta uninit this someplace */
    REGION_NULL(pScreen, &pPriv->clip);
#endif

    /* With LFP's we need to detect whether we're in One Line Mode, which
     * essentially means a resolution greater than 1024x768, and fix up
     * the scaler accordingly. */
    pPriv->scaleRatio = 0x10000;
    pPriv->oneLineMode = FALSE;

    ret = wsbmGenBuffers(driver_data->main_pool, 1,
                         &pPriv->wsbo, 0,
                         WSBM_PL_FLAG_TT);
    if (ret)
        goto out_err_ppriv;

    ret = wsbmBOData(pPriv->wsbo,
                     4096,
                     NULL, NULL,
                     WSBM_PL_FLAG_TT);

    if (ret)
        goto out_err_bo;

    pPriv->regmap = wsbmBOMap(pPriv->wsbo, WSBM_ACCESS_READ | WSBM_ACCESS_WRITE);
    if (!pPriv->regmap) {
        goto out_err_bo;
    }

    return pPriv;

  out_err_bo:
    wsbmBOUnreference(&pPriv->wsbo);

  out_err_ppriv:
    free(pPriv);
  out_err:
    return NULL;
}

int
psbInitVideo(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    PsbPortPrivPtr pPriv;

    pPriv = psbSetupImageVideoOverlay(ctx);

    if (pPriv) {
        I830ResetVideo(pPriv);
        I830UpdateGamma(ctx, pPriv);
        driver_data->dri_priv = pPriv;
        return 0;
    }
    return -1; // error
}

int
psbDeInitVideo(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    PsbPortPrivPtr pPriv;

    pPriv = (PsbPortPrivPtr)(driver_data->dri_priv);
    if (pPriv)
        psbPortPrivDestroy(ctx, pPriv);

    return 0;
}


static int psb__CheckPutSurfaceXvPort(
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
)
{
    INIT_DRIVER_DATA;
    object_surface_p obj_surface = SURFACE(surface);
    uint32_t buf_pl;
    
    /* silent klockwork */
    if (obj_surface && obj_surface->psb_surface)
        buf_pl = obj_surface->psb_surface->buf.pl_flags;
    else
        return -1;
    
    if (flags & VA_CLEAR_DRAWABLE)
        return 0;

    if (((buf_pl & (WSBM_PL_FLAG_TT | DRM_PSB_FLAG_MEM_RAR | DRM_PSB_FLAG_MEM_CI)) == 0) /* buf not in TT/RAR or CI */
        || (obj_surface->width > 1920)  /* overlay can't support subpicture */
        || (obj_surface->subpic_count > 0)  /* overlay can't support subpicture */
        ) {
        psb__error_message("ERROR: %s() Overlay can not support subpicture or width>1920",__func__);
        return -1;
    }
        
    return 0;
}

static int last_num_clipbox = 0;
static VARectangle last_clipbox[16];

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
    ){
    INIT_DRIVER_DATA;
    psb_output_p output = GET_OUTPUT_DATA(ctx);
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_surface_p obj_surface = SURFACE(surface);
    psb_surface_p psb_surface = obj_surface->psb_surface;

    int i=0;
    int id;
#ifndef ANDROID
    int display_width  = (int)(DisplayWidth(ctx->native_dpy, DefaultScreen(ctx->native_dpy))) - 1;
    int display_height = (int)(DisplayHeight(ctx->native_dpy, DefaultScreen(ctx->native_dpy))) - 1;
#else
    int display_width = 1024;
    int display_height = 600;
#endif
    int x11_window_width, x11_window_height;

    if (NULL == psb_surface) {
	psb__error_message("%s L%d Invalide surface handle\n", __FUNCTION__, __LINE__);
	return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    pthread_mutex_lock(&output->output_mutex);
    psb__CheckPutSurfaceXvPort(ctx, surface, draw,
                               srcx, srcy, srcw, srch,
                               destx, desty, destw, desth,
                               cliprects, number_cliprects, flags);

    if (destx < 0) {
        x11_window_width = destw + destx;
        srcx = -destx;
        destx = 0;
    } else
        x11_window_width = destw;
    
    if (desty < 0) {
        x11_window_height = desth + desty;
        srcy = -desty;
        desty = 0;
    } else
        x11_window_height = desth;

    if ((destx + x11_window_width) > display_width)
        x11_window_width = display_width - destx;

    if ((desty + x11_window_height) > display_height)
        x11_window_height = display_height - desty;

    /* TODO zhaohan: Currently Android color key is set by test app*/
#ifndef ANDROID
    if (output->output_drawable != draw) 
    {
        PsbPortPrivRec *pPriv = (PsbPortPrivPtr)(driver_data->dri_priv);
        
        output->output_drawable = draw;
        if (output->gc)
            XFreeGC((Display *)ctx->native_dpy, output->gc);
	output->gc = XCreateGC ((Display *)ctx->native_dpy, draw, 0, NULL);

        /* paint the color key */
        XSetForeground((Display *)ctx->native_dpy, output->gc, pPriv->colorKey);
        XFillRectangle((Display *)ctx->native_dpy, draw, output->gc, 0, 0, x11_window_width, x11_window_height);
        XSync((Display *)ctx->native_dpy, False);
    }

    if ((number_cliprects != last_num_clipbox) ||
        (memcmp(&cliprects[0], &last_clipbox[0], (number_cliprects > 16 ? 16: number_cliprects)*sizeof(VARectangle))!=0)) {
        if (number_cliprects <= 1) {
            XFillRectangle((Display *)ctx->native_dpy, draw, output->gc, 0, 0, x11_window_width, x11_window_height);
        } else {
            for (i = 0; i < number_cliprects; i++) {
                psb__error_message("cliprect # %d @(%d, %d) width %d height %d\n", i, 
                                   cliprects[i].x, cliprects[i].y,
                                   cliprects[i].width, cliprects[i].height);
                XFillRectangle((Display *)ctx->native_dpy, draw, output->gc, 
                               cliprects[i].x - destx, cliprects[i].y - desty,
                               cliprects[i].width, cliprects[i].height);
            }
        }
        last_num_clipbox = number_cliprects;
        memcpy(&last_clipbox[0], &cliprects[0],(number_cliprects > 16 ? 16 : number_cliprects)*sizeof(VARectangle));
        
        XSync((Display *)ctx->native_dpy, False);
    }
#endif /* endif Android */

    if (flags & VA_CLEAR_DRAWABLE) {
	I830StopVideo(ctx);
        return 0;
    }

    if (flags & VA_CLEAR_DRAWABLE) {
        psb__information_message("Clean draw with color 0x%08x\n",output->clear_color); 
        driver_data->cur_displaying_surface = VA_INVALID_SURFACE;
        driver_data->last_displaying_surface = VA_INVALID_SURFACE;
        obj_surface->display_timestamp = 0;

        return vaStatus;
    }

    /* FIXME: Set NV12 as default */
    id = VA_FOURCC_NV12;

#ifdef ANDROID /* USE_FIT_SCR_SIZE */
    /* calculate fit screen size of frame */
    unsigned short _scr_x = 1024;
    unsigned short _scr_y = 600;
    float _slope_xy = (float)srch/srcw;
    unsigned short _destw = (short)(_scr_y/_slope_xy);
    unsigned short _desth = (short)(_scr_x*_slope_xy);
    short _pos_x, _pos_y;
    if (_destw <= _scr_x) {
        _desth = _scr_y;
        _pos_x = (_scr_x-_destw)>>1;
        _pos_y = 0;
    } else {
        _destw = _scr_x;
        _pos_x = 0;
        _pos_y = (_scr_y-_desth)>>1;
    }
    destx += _pos_x;
    desty += _pos_y;
    destw = _destw;
    desth = _desth;
    /* psb_force_dpms_on(ctx); */
    I830PutImage(ctx, surface, srcx, srcy, srcw, srch, destx, desty, destw, desth, id, flags);
#else
    I830PutImage(ctx, surface, srcx, srcy, srcw, srch, destx, desty, x11_window_width, x11_window_height, id, flags);
#endif


    if (driver_data->cur_displaying_surface != VA_INVALID_SURFACE)
        driver_data->last_displaying_surface = driver_data->cur_displaying_surface;
    obj_surface->display_timestamp = 0;
    driver_data->cur_displaying_surface = surface;

    pthread_mutex_unlock(&output->output_mutex);
    return vaStatus;
}
