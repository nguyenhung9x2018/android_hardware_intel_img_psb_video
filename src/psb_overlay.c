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

#if USE_PVR2D
#include <pvr2d.h>
#include <pvr_android.h>
#define PAGE_SHIFT 12
#define ALIGN_TO(_arg, _align) \
  (((_arg) + ((_align) - 1)) & ~((_align) - 1))
#endif

#define LOG_TAG "psb_overlay"
#include <cutils/log.h>

#define INIT_DRIVER_DATA psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;

typedef struct _PsbPortPrivRec
{
#if USE_PVR2D
    PVR2DMEMINFO *videoBuf[2];
    uint32_t videoBuf0_gtt_offset;
    uint32_t videoBuf1_gtt_offset;
#endif
    struct _WsbmBufferObject *vaSrf;
    struct _WsbmBufferObject *rotateSrf;
    int curBuf;
#if USE_PVR2D
    unsigned int videoBufSize;
#endif
    float conversionData[11];
    Bool hdtv;

    /* used to check downscale*/
    short width_save;
    short height_save;

    /* information of display attribute */
    ov_psb_fixed32 brightness;
    ov_psb_fixed32 contrast;
    ov_psb_fixed32 saturation;
    ov_psb_fixed32 hue;

    /* gets set by any changes to Hue, Brightness,saturation, hue or csc matrix. */
    psb_coeffs_s coeffs;
    unsigned long sgx_coeffs[9];

    unsigned int src_nominalrange;
    unsigned int dst_nominalrange;
    unsigned int video_transfermatrix;

    /* hwoverlay */
    uint32_t gamma0;
    uint32_t gamma1;
    uint32_t gamma2;
    uint32_t gamma3;
    uint32_t gamma4;
    uint32_t gamma5;
    uint32_t colorKey;
    Bool overlayOK;
    int oneLineMode;
    int scaleRatio;
    Rotation rotation;

#if USE_PVR2D
    PVR2DMEMINFO *wsbo;
    PVR2DCONTEXTHANDLE hPVR2DContext;
    uint32_t wsbo_gtt_offset;
#else
    struct _WsbmBufferObject *wsbo;
#endif
    uint32_t YBuf0offset;
    uint32_t UBuf0offset;
    uint32_t VBuf0offset;
    uint32_t YBuf1offset;
    uint32_t UBuf1offset;
    uint32_t VBuf1offset;
    unsigned char *regmap;
} PsbPortPrivRec, *PsbPortPrivPtr;

/*
 * YUV to RBG conversion.
 */

static float yOffset = 16.f;
static float yRange = 219.f;
static float videoGamma = 2.8f;

/*
 * The ITU-R BT.601 conversion matrix for SDTV.
 */

static float bt_601[] = {
    1.0, 0.0, 1.4075,
    1.0, -0.3455, -0.7169,
    1.0, 1.7790, 0.
};

/*
 * The ITU-R BT.709 conversion matrix for HDTV.
 */

static float bt_709[] = {
    1.0, 0.0, 1.581,
    1.0, -0.1881, -0.47,
    1.0, 1.8629, 0.
};

#if USE_PVR2D
int PVRDRMGttMap(int fd, IMG_HANDLE hMemInfo, uint32_t * offset)
{
        struct psb_gtt_mapping_arg arg;
        int res;

        arg.hKernelMemInfo = hMemInfo;

        res = drmCommandWriteRead(fd, DRM_PSB_GTT_MAP, &arg, sizeof(arg));
        if(!res) {
                *offset = arg.offset_pages;
        }

        return res;
}

int PVRDRMGttUnmap(int fd, IMG_HANDLE hMemInfo)
{
        struct psb_gtt_mapping_arg arg;
        int res;

        arg.hKernelMemInfo = hMemInfo;

        res = drmCommandWrite(fd, DRM_PSB_GTT_UNMAP, &arg, sizeof(arg));

        return res;
}
#endif

static void
psbSetupConversionData(PsbPortPrivPtr pPriv, Bool hdtv)
{
    if (pPriv->hdtv != hdtv) {
        int i;

        if (hdtv)
            memcpy(pPriv->conversionData, bt_709, sizeof(bt_709));
        else
            memcpy(pPriv->conversionData, bt_601, sizeof(bt_601));

        for (i = 0; i < 9; ++i) {
            pPriv->conversionData[i] /= yRange;
        }

        /*
         * Adjust for brightness, contrast, hue and saturation here.
         */

        pPriv->conversionData[9] = -yOffset;
        /*
         * Not used ATM
         */
        pPriv->conversionData[10] = videoGamma;
        pPriv->hdtv = hdtv;
    }
}

static void psb_setup_coeffs(PsbPortPrivPtr);
static void psb_pack_coeffs(PsbPortPrivPtr, unsigned long *);

static void
psbSetupPlanarConversionData(PsbPortPrivPtr pPriv, Bool hdtv)
{
    psb_setup_coeffs(pPriv);
    psb_pack_coeffs(pPriv, &pPriv->sgx_coeffs[0]);
}

static PsbPortPrivPtr
psbPortPrivCreate(void)
{
    PsbPortPrivPtr pPriv;
    int ret;

    pPriv = calloc(1, sizeof(*pPriv));
    if (!pPriv)
        return NULL;

#if USE_PVR2D
    ret = pvr_android_context_create(&pPriv->hPVR2DContext);
    if (ret < 0) {
        LOGE("%s(): null PVR context!!", __func__);
        free(pPriv);
        return NULL;
    }
#endif

#if 0
    REGION_NULL(pScreen, &pPriv->clip);
#endif
    pPriv->hdtv = TRUE;
    psbSetupConversionData(pPriv, FALSE);

    /* coeffs defaut value */
    pPriv->brightness.Value = OV_BRIGHTNESS_DEFAULT_VALUE;
    pPriv->brightness.Fraction = 0;

    pPriv->contrast.Value = OV_CONTRAST_DEFAULT_VALUE;
    pPriv->contrast.Fraction = 0;
    pPriv->hue.Value = OV_HUE_DEFAULT_VALUE;
    pPriv->hue.Fraction = 0;
    pPriv->saturation.Value = OV_SATURATION_DEFAULT_VALUE;
    pPriv->saturation.Fraction = 0;

    pPriv->src_nominalrange = PSB_NominalRange_0_255;
    pPriv->dst_nominalrange = PSB_NominalRange_0_255;
    pPriv->video_transfermatrix = PSB_VideoTransferMatrix_BT709;

    /* FIXME: is this right? set up to current screen size */
#if 1
    pPriv->width_save = 1024;
    pPriv->height_save = 600;
#endif

    psbSetupPlanarConversionData(pPriv, FALSE);

    return pPriv;
}

static void
psbStopVideo(VADriverContextP ctx, PsbPortPrivPtr pPriv)
{
    INIT_DRIVER_DATA;
#if USE_PVR2D
    if (pPriv->videoBuf[0])
    {
        PVRDRMGttUnmap(driver_data->drm_fd, pPriv->videoBuf[0]->hPrivateMapData);
        PVR2DMemFree(pPriv->hPVR2DContext, pPriv->videoBuf[0]);
        pPriv->videoBuf[0] = NULL;
    }

    if (pPriv->videoBuf[1])
    {
        PVRDRMGttUnmap(driver_data->drm_fd, pPriv->videoBuf[1]->hPrivateMapData);
        PVR2DMemFree(pPriv->hPVR2DContext, pPriv->videoBuf[1]);
        pPriv->videoBuf[1] = NULL;
    }
#endif
    if (pPriv->vaSrf)
        wsbmBOUnreference(&pPriv->vaSrf);
#if 0
    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
#endif
}

static void
psbPortPrivDestroy(VADriverContextP ctx, PsbPortPrivPtr pPriv)
{
    INIT_DRIVER_DATA;

    if (!pPriv)
        return;

    psbStopVideo(ctx, pPriv);
    I830StopVideo(ctx);

#if USE_PVR2D
        if (pPriv->wsbo) {
            PVRDRMGttUnmap(driver_data->drm_fd, pPriv->wsbo->hPrivateMapData);
            PVR2DMemFree(pPriv->hPVR2DContext, pPriv->wsbo);
            pPriv->wsbo = NULL;
        }
#else
        wsbmBOUnreference(&pPriv->wsbo);
#endif

    free(pPriv);
}

static PsbPortPrivPtr
psbSetupImageVideoOverlay(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    PsbPortPrivPtr pPriv;
    int ret;
    int i;
#if USE_PVR2D
    PVR2DERROR ePVR2DStatus;
#endif

    pPriv = psbPortPrivCreate();
    if (!pPriv)
        goto out_err;

    /* use green as color key by default for android media player */
    pPriv->colorKey = 0x0440;

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

    /*
     * Initialise pPriv->overlayOK.  Set it to TRUE here so that a warning will
     * be generated if i830_crtc_dpms_video() sets it to FALSE during mode
     * setup.
     */
    pPriv->overlayOK = TRUE;

#if USE_PVR2D
    ePVR2DStatus = PVR2DMemAlloc(pPriv->hPVR2DContext, 4096, 0, 0, &pPriv->wsbo);
    if (ePVR2DStatus != PVR2D_OK)
    {
        LOGE("%s: PVR2DMemAlloc failed to create video buffer\n", __func__);
        goto out_err_ppriv;
    }

    pPriv->regmap = (unsigned char *)pPriv->wsbo->pBase;
    if (!pPriv->regmap)
    {
        LOGE("%s: Bad overlay address\n", __func__);
        goto out_err_bo;
    }

    ret = PVRDRMGttMap(driver_data->drm_fd, pPriv->wsbo->hPrivateMapData, &pPriv->wsbo_gtt_offset);
    if (ret)
    {
        LOGE("%s: Can't map overlay buffer to GTT\n", __func__);
        goto out_err_bo;
    }
#else
    ret = wsbmGenBuffers(driver_data->main_pool, 1,
                         &pPriv->wsbo, 0,
                         WSBM_PL_FLAG_VRAM |
                         WSBM_PL_FLAG_NO_EVICT);
    if (ret)
        goto out_err_ppriv;
    ret = wsbmBOData(pPriv->wsbo,
                     4096,
                     NULL, NULL,
                     WSBM_PL_FLAG_VRAM |
                     WSBM_PL_FLAG_NO_EVICT);
    if (ret) {
        goto out_err_bo;
    }

    pPriv->regmap = wsbmBOMap(pPriv->wsbo, WSBM_ACCESS_READ | WSBM_ACCESS_WRITE);
    if (!pPriv->regmap) {
        goto out_err_bo;
    }
#endif

    return pPriv;

  out_err_bo:
#if USE_PVR2D
    PVR2DMemFree(pPriv->hPVR2DContext, pPriv->wsbo);
    pPriv->wsbo = NULL;
#else
    wsbmBOUnreference(&pPriv->wsbo);
#endif
  out_err_ppriv:
    free(pPriv);
  out_err:
    return NULL;
}

int
psbMoveInVideo(PsbPortPrivPtr pPriv)
{
    int ret;

#if USE_PVR2D
    pPriv->regmap = (unsigned char *)pPriv->wsbo->pBase;
    if (!pPriv->regmap)
    {
        pPriv->overlayOK = FALSE;
        ret = -1;
        LOGE("%s: Bad overlay address\n", __func__);
    } else {
        ret = 0;
    }
#else
    ret = wsbmBOSetStatus(pPriv->wsbo,
                    WSBM_PL_FLAG_VRAM |
                    WSBM_PL_FLAG_NO_EVICT,
                    WSBM_PL_FLAG_SYSTEM);

    if (ret) {
        pPriv->overlayOK = FALSE;
        return ret;
    }

    pPriv->regmap = wsbmBOMap(pPriv->wsbo, WSBM_ACCESS_READ | WSBM_ACCESS_WRITE);

    if (!pPriv->regmap)
        pPriv->overlayOK = FALSE;
#endif

    return ret;
}

int
psbMoveOutVideo(VADriverContextP ctx, PsbPortPrivPtr pPriv)
{
    INIT_DRIVER_DATA;
    int ret;

#if USE_PVR2D
    if (pPriv->videoBuf[0]) {
        PVRDRMGttUnmap(driver_data->drm_fd, pPriv->videoBuf[0]->hPrivateMapData);
        PVR2DMemFree(pPriv->hPVR2DContext, pPriv->videoBuf[0]);
        pPriv->videoBuf[0] = NULL;
    }

    if (pPriv->videoBuf[1]) {
        PVRDRMGttUnmap(driver_data->drm_fd, pPriv->videoBuf[1]->hPrivateMapData);
        PVR2DMemFree(pPriv->hPVR2DContext, pPriv->videoBuf[1]);
        pPriv->videoBuf[1] = NULL;
    }

    pPriv->videoBufSize = 0;
    ret = 0;
#else
    wsbmBOUnmap(pPriv->wsbo);
    pPriv->regmap = NULL;
    ret = wsbmBOSetStatus(pPriv->wsbo, WSBM_PL_FLAG_SYSTEM,
                                 WSBM_PL_FLAG_VRAM |
                                 WSBM_PL_FLAG_NO_EVICT);
#endif
    return ret;
}

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
    overlay->DCLRKM = 0x070307 | DEST_KEY_ENABLE;
#else
    overlay->DCLRKM &= ~DEST_KEY_ENABLE;
#endif

    overlay->OCONFIG = CC_OUT_8BIT;

    //MRST
    overlay->OCONFIG |= OVERLAY_PIPE_A;
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

#if 0
    /*BRD ErrorF ("Original gamma: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx\n",
            gamma0, gamma1, gamma2, gamma3, gamma4, gamma5);*/
#endif
    gamma1 = I830BoundGamma (gamma1, gamma0);
    gamma2 = I830BoundGamma (gamma2, gamma1);
    gamma3 = I830BoundGamma (gamma3, gamma2);
    gamma4 = I830BoundGamma (gamma4, gamma3);
    gamma5 = I830BoundGamma (gamma5, gamma4);
#if 0
    /*BRD ErrorF ("Bounded  gamma: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx\n",
            gamma0, gamma1, gamma2, gamma3, gamma4, gamma5);*/
#endif

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

void
I830StopVideo(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    PsbPortPrivPtr pPriv;

    pPriv = (PsbPortPrivPtr)(driver_data->dri_priv);

#if USE_PVR2D
    uint32_t offset = pPriv->wsbo_gtt_offset << PAGE_SHIFT;
#else
    long offset = wsbmBOOffsetHint(pPriv->wsbo) & 0x0FFFFFFF;
#endif
    struct drm_psb_register_rw_arg regs;

    if (pPriv->vaSrf)
        wsbmBOUnreference(&pPriv->vaSrf);

    if (pPriv->rotateSrf)
        wsbmBOUnreference(&pPriv->rotateSrf);

#if 0
    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
#endif

    I830ResetVideo(pPriv);

    /* FIXME what is this? */
    /* if(IS_PSB(pDevice)) */
    /* offset += pDevice->stolenBase;*/
#if 0
        pci_read_config_dword(dev->pdev, PSB_BSM, &pg->stolen_base);
        vram_stolen_size = pg->gtt_phys_start - pg->stolen_base - PAGE_SIZE;

        /* CI is not included in the stolen size since the TOPAZ MMU bug */
        ci_stolen_size = dev_priv->ci_region_size;
        /* Don't add CI & RAR share buffer space managed by TTM to stolen_size */
        stolen_size = vram_stolen_size;
#endif

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

static int
psbVaCheckVideoBuffer(VADriverContextP ctx, PsbPortPrivPtr pPriv,
                      unsigned int pl, PsbXvVAPutSurfacePtr vaPtr,
		      unsigned int rotate)
{
    INIT_DRIVER_DATA;
    int ret;

    if (pPriv->vaSrf) {
        /* wsbmBOSetStatus(pPriv->vaSrf, 0, WSBM_PL_FLAG_NO_EVICT); */
        wsbmBOUnreference(&pPriv->vaSrf);
        pPriv->vaSrf = NULL;
    }

    /* regenerate a new buffer for new surface */
    ret = wsbmGenBuffers(driver_data->main_pool, 1,
                         &pPriv->vaSrf, 0,
                         pl);
    if (!pPriv->vaSrf)
        return BadAlloc;

    ret = wsbmBOSetReferenced(pPriv->vaSrf, vaPtr->src_srf.bufid);

    if (ret)
        return BadAlloc;

#if 0
    pPriv->srf[0][0].buffer = pPriv->vaSrf;
    pPriv->srf[0][0].offset = 0;
    pPriv->srf[0][1].buffer = pPriv->vaSrf;
    pPriv->srf[0][1].offset = 0;
#endif

    if (rotate) {
        if (pPriv->rotateSrf) {
            /* wsbmBOSetStatus(pPriv->spriteSrf, 0, WSBM_PL_FLAG_NO_EVICT); */
            wsbmBOUnreference(&pPriv->rotateSrf);
            pPriv->rotateSrf = NULL;
        }

        /* regenerate a new buffer for new surface */
        ret = wsbmGenBuffers(driver_data->main_pool, 1,
                             &pPriv->rotateSrf, 0,
                             pl);
        if (!pPriv->rotateSrf)
            return BadAlloc;

        ret = wsbmBOSetReferenced(pPriv->rotateSrf, vaPtr->dst_srf.bufid);

        if (ret)
            return BadAlloc;
    }

    return Success;
}

static int
psbCheckVideoBuffer(VADriverContextP ctx, PsbPortPrivPtr pPriv, unsigned int size)
{
    INIT_DRIVER_DATA;
    int ret;
    size = ALIGN_TO(size, 4096);

#if USE_PVR2D
    PVR2DERROR ePVR2DStatus;

    if (pPriv->videoBufSize != size) {
        PVR2DMemFree(pPriv->hPVR2DContext, pPriv->videoBuf[0]);
        PVR2DMemFree(pPriv->hPVR2DContext, pPriv->videoBuf[1]);
        pPriv->videoBuf[0] = NULL;
        pPriv->videoBuf[1] = NULL;
    }
    if (!pPriv->videoBuf[0]) {
        ePVR2DStatus = PVR2DMemAlloc(pPriv->hPVR2DContext,
                                     size, 0, 0, &pPriv->videoBuf[0]);
        if (ePVR2DStatus != PVR2D_OK) {
            LOGE("%s: PVR2DMemAlloc failed to create video buffer 0\n", __func__);
            pPriv->videoBuf[0] = NULL;
            return BadAlloc;
        }

        ePVR2DStatus = PVR2DMemAlloc(pPriv->hPVR2DContext,
                                     size, 0, 0, &pPriv->videoBuf[1]);
        if (ePVR2DStatus != PVR2D_OK) {
            LOGE("%s: PVR2DMemAlloc failed to create video buffer 1\n", __func__);
            pPriv->videoBuf[1] = NULL;
            return BadAlloc;
        }


        ret = PVRDRMGttMap(driver_data->drm_fd, pPriv->videoBuf[0]->hPrivateMapData, &pPriv->videoBuf0_gtt_offset);
        if (ret) {
            LOGE("%s: Failed to map video buffer 0 into GTT.\n", __func__);
            PVR2DMemFree(pPriv->hPVR2DContext, pPriv->videoBuf[0]);
            pPriv->videoBuf[0] = NULL;
            return BadAlloc;
        }

        ret = PVRDRMGttMap(driver_data->drm_fd, pPriv->videoBuf[1]->hPrivateMapData, &pPriv->videoBuf1_gtt_offset);
        if (ret) {
            LOGE("%s: Failed to map video buffer 1 into GTT.\n", __func__);
            PVR2DMemFree(pPriv->hPVR2DContext, pPriv->videoBuf[1]);
            pPriv->videoBuf[1] = NULL;
            return BadAlloc;
        }

        pPriv->videoBufSize = size;
#if 0
        pPriv->srf[0][0].buffer = pPriv->videoBuf[0];
        pPriv->srf[0][0].offset = 0;
        pPriv->srf[0][1].buffer = pPriv->videoBuf[1];
        pPriv->srf[0][1].offset = 0;
#endif
    }
#else /* USE_PVR2D */
#error "psbCheckVideoBuffer(): pls implement this."
#endif /* USE_PVR2D */
    return Success;
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
i830_display_video(VADriverContextP ctx, PsbPortPrivPtr pPriv, VASurfaceID surface,
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
#if USE_PVR2D
    uint32_t offset = pPriv->wsbo_gtt_offset << PAGE_SHIFT;
#else
    unsigned int offset = wsbmBOOffsetHint(pPriv->wsbo) & 0x0FFFFFFF;
#endif
    I830OverlayRegPtr overlay = (I830OverlayRegPtr)(pPriv->regmap);
    struct drm_psb_register_rw_arg regs;

    if (!pPriv->overlayOK)
        return;

    //FIXME(Ben):There is a hardware bug which prevents overlay from
    //           being reenabled after being disabled.  Until this is
    //           fixed, don't disable the overlay.  We just make it
    //           fully transparent and set it's window size to zero.
    //           once hardware is fixed, remove this line disabling
    //           CONST_ALPHA_ENABLE.
//    if(IS_MRST(pDevice))
        overlay->DCLRKM &= ~CONST_ALPHA_ENABLE;

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
    case FOURCC_XVVA:
        overlay->SWIDTH = width | ((width/2 & 0x7ff) << 16);
        swidthy = i830_swidth (offsety, width, mask, shift);
        swidthuv = i830_swidth (offsetu, width/2, mask, shift);
        overlay->SWIDTHSW = (swidthy) | (swidthuv << 16);
        overlay->SHEIGHT = height | ((height / 2) << 16);
        break;
    case FOURCC_NV12:
        overlay->SWIDTH = width | ((width/2 & 0x7ff) << 16);
        swidthy = i830_swidth (offsety, width, mask, shift);
        swidthuv = i830_swidth (offsetu, width/2, mask, shift);
        overlay->SWIDTHSW = (swidthy) | (swidthuv << 16);
        overlay->SHEIGHT = height | ((height / 2) << 16);
        break;
    case FOURCC_YV12:
    case FOURCC_I420:
        overlay->SWIDTH = width | ((width/2 & 0x7ff) << 16);
        swidthy  = i830_swidth (offsety, width, mask, shift);
        swidthuv = i830_swidth (offsetu, width/2, mask, shift);
        overlay->SWIDTHSW = (swidthy) | (swidthuv << 16);
        overlay->SHEIGHT = height | ((height / 2) << 16);
        break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
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

    {
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
        if ((id == FOURCC_XVVA) && (0 != (flags & (VA_TOP_FIELD | VA_BOTTOM_FIELD ))))
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

        newval = (xscaleInt << 16) |
        ((xscaleFract & 0xFFF) << 3) | ((yscaleFract & 0xFFF) << 20);
        if (newval != overlay->YRGBSCALE) {
            scaleChanged = TRUE;
            overlay->YRGBSCALE = newval;
        }

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
    case FOURCC_XVVA:
        overlay->OSTRIDE = dstPitch | (dstPitch << 16);
        OCMD &= ~SOURCE_FORMAT;
        OCMD &= ~OV_BYTE_ORDER;
        OCMD |= NV12;//in the spec, there are two NV12, which to use?
        break;
    case FOURCC_NV12:
        overlay->OSTRIDE = dstPitch | (dstPitch << 16);
        OCMD &= ~SOURCE_FORMAT;
        OCMD &= ~OV_BYTE_ORDER;
        OCMD |= NV12;//in the spec, there are two NV12, which to use?
        break;
    case FOURCC_YV12:
    case FOURCC_I420:
#if 0
        /* set UV vertical phase to -0.25 */
        overlay->UV_VPH = 0x30003000;
#endif
        overlay->OSTRIDE = (dstPitch * 2) | (dstPitch << 16);
        OCMD &= ~SOURCE_FORMAT;
        OCMD &= ~OV_BYTE_ORDER;
        OCMD |= YUV_420;
        break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
        overlay->OSTRIDE = dstPitch;
        OCMD &= ~SOURCE_FORMAT;
        OCMD |= YUV_422;
        OCMD &= ~OV_BYTE_ORDER;
        if (id == FOURCC_UYVY)
            OCMD |= Y_SWAP;
        break;
    }

#if 1
    if (id == FOURCC_XVVA)
    {
        if (flags & (VA_TOP_FIELD | VA_BOTTOM_FIELD ))
        {
            OCMD |= BUF_TYPE_FIELD;
            OCMD &= ~FIELD_SELECT;

            if (flags & VA_BOTTOM_FIELD)
            {
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
        }
        else
        {
            OCMD &= ~(FIELD_SELECT);
            OCMD &= ~BUF_TYPE_FIELD;
        }
    }
    else
        OCMD &= ~(FIELD_SELECT);
#else
            OCMD &= ~(FIELD_SELECT);
            OCMD &= ~BUF_TYPE_FIELD;
#endif

    OCMD &= ~(BUFFER_SELECT);

    if (pPriv->curBuf == 0)
        OCMD |= BUFFER0;
    else
        OCMD |= BUFFER1;

    overlay->OCMD = OCMD;

    /* make sure the overlay is on */
//    if(IS_PSB(pDevice))
//      offset += pDevice->stolenBase;

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
int
I830PutImage(VADriverContextP ctx,
             VASurfaceID surface,
             short src_x, short src_y,
             short src_w, short src_h,
             short drw_x, short drw_y,
             short drw_w, short drw_h, int id)
{
    INIT_DRIVER_DATA;
    psb_output_p output = GET_OUTPUT_DATA(ctx);
    int x1, x2, y1, y2;
    int width, height;
    int srcPitch = 0, srcPitch2 = 0, dstPitch, destId, Y_dstpitch;
    int top, left, npixels, nlines, size;
    int pitchAlignMask;
    int ret;
    BoxRec dstBox;
    PsbPortPrivPtr pPriv;
    PsbXvVAPutSurfacePtr vaPtr;

    pPriv = (PsbPortPrivPtr)(driver_data->dri_priv);
    vaPtr = (PsbXvVAPutSurfacePtr)&output->imgdata_vasrf;

    switch (id) {
        case FOURCC_NV12:
            width = vaPtr->src_srf.width;
            height = vaPtr->src_srf.height;
            break;
        default:
            width = vaPtr->src_srf.width;
            height = vaPtr->src_srf.height;
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
        return Success;
    }
#endif

    destId = id;
    switch (id) {
    case FOURCC_XVVA:
        srcPitch = (vaPtr->src_srf.stride + 0x3) & ~0x3;
        srcPitch2 = (vaPtr->src_srf.stride + 0x3) & ~0x3;
        break;
    case FOURCC_NV12:
        srcPitch = (width + 0x3) & ~0x3;
        srcPitch2 = (width + 0x3) & ~0x3;
        break;
    case FOURCC_YV12:
    case FOURCC_I420:
        srcPitch = (width + 0x3) & ~0x3;
        srcPitch2 = ((width >> 1) + 0x3) & ~0x3;
        break;
#if USE_DISPLAY_C_SPRITE
    case FOURCC_RGBA:
        srcPitch = width << 2;
        break;
#endif
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
        srcPitch = width << 1;
        break;
    }

    pitchAlignMask = 63;

#if USE_ROTATION_FUNC
    /*
     *Determine the desired destination pitch (representing the chroma's pitch,
     * in the planar case.
     */
    switch (destId) {
    case FOURCC_XVVA:
        if (pPriv->rotation != RR_Rotate_0) {
            dstPitch = vaPtr->dst_srf.stride;
            size = vaPtr->dst_srf.size;
        } else {
            dstPitch = (vaPtr->src_srf.stride + pitchAlignMask) & ~pitchAlignMask;
            size = dstPitch * height + 2 * (dstPitch / 2) * (height / 2);
        }
        break;
    case FOURCC_NV12:
        if (pPriv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
            dstPitch = (height + pitchAlignMask) & ~pitchAlignMask;
            size = dstPitch * width * 3 / 2;
        } else {
            dstPitch = (width + pitchAlignMask) & ~pitchAlignMask;
            size = dstPitch * height + 2 * (dstPitch / 2) * (height / 2);
        }
        break;
#if USE_DISPLAY_C_SPRITE
    case FOURCC_RGBA:
        dstPitch = width << 2;
        size = dstPitch * height;
        break;
#endif
    case FOURCC_YV12:
    case FOURCC_I420:
        if (pPriv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
            dstPitch = ((height / 2) + pitchAlignMask) & ~pitchAlignMask;
            size = dstPitch * width * 3;
        } else {
            dstPitch = ((width / 2) + pitchAlignMask) & ~pitchAlignMask;
            size = dstPitch * height * 3;
        }
        break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
        if (pPriv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
            dstPitch = ((height << 1) + pitchAlignMask) & ~pitchAlignMask;
            size = dstPitch * width;
        } else {
            dstPitch = ((width << 1) + pitchAlignMask) & ~pitchAlignMask;
            size = dstPitch * height;
        }
        break;
    default:
        dstPitch = 0;
        size = 0;
        break;
    }
#else
    dstPitch = (vaPtr->src_srf.stride + pitchAlignMask) & ~pitchAlignMask;
    size = dstPitch * height + 2 * (dstPitch / 2) * (height / 2);
#endif

    if (id != FOURCC_XVVA)
        ret = psbCheckVideoBuffer(ctx, pPriv, size);
    else
        ret = psbVaCheckVideoBuffer(ctx, pPriv,
                             vaPtr->src_srf.pl_flags | WSBM_PL_FLAG_NO_EVICT,
                             vaPtr, pPriv->rotation != RR_Rotate_0);
    if (ret)
        return ret;

    /* copy data */
    top = y1 >> 16;
    left = (x1 >> 16) & ~1;
    npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;

    if ((id == FOURCC_NV12) || (id == FOURCC_XVVA))
        Y_dstpitch = dstPitch;
    else
        Y_dstpitch = 2 * dstPitch;

#if USE_ROTATION_FUNC
    if (id == FOURCC_XVVA) {
        if (pPriv->rotation != RR_Rotate_0) {
#if USE_DISPLAY_C_SPRITE
            if (vaPtr->dst_srf.fourcc == VA_FOURCC_RGBA) {
                sprite_offset = (wsbmBOOffsetHint(pPriv->rotateSrf) & 0x0FFFFFFF) + vaPtr->dst_srf.pre_add;
            }
            else
#endif
            {
                pPriv->YBuf0offset = vaPtr->dst_srf.pre_add +
                    (wsbmBOOffsetHint(pPriv->rotateSrf) & 0x0FFFFFFF) + top * dstPitch + left;
                pPriv->YBuf1offset = pPriv->YBuf0offset;
                if (pPriv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
                    pPriv->UBuf0offset = pPriv->YBuf0offset + (dstPitch  * width);
                    pPriv->VBuf0offset = pPriv->UBuf0offset;
                    pPriv->UBuf1offset = pPriv->UBuf0offset;
                    pPriv->VBuf1offset = pPriv->UBuf0offset;
                } else {
                    pPriv->UBuf0offset = pPriv->YBuf0offset + (dstPitch  * height);
                    pPriv->VBuf0offset = pPriv->UBuf0offset;
                    pPriv->UBuf1offset = pPriv->UBuf0offset;
                    pPriv->VBuf1offset = pPriv->UBuf0offset;
                }
            }
        } else {
            pPriv->YBuf0offset = vaPtr->src_srf.pre_add +
                 (wsbmBOOffsetHint(pPriv->vaSrf) & 0x0FFFFFFF) + top * dstPitch + left;
            pPriv->YBuf1offset = vaPtr->src_srf.pre_add +
                 (wsbmBOOffsetHint(pPriv->vaSrf) & 0x0FFFFFFF) + top * dstPitch + left;
            pPriv->UBuf0offset = vaPtr->src_srf.pre_add +
                 (wsbmBOOffsetHint(pPriv->vaSrf) & 0x0FFFFFFF) + (Y_dstpitch  * height) +
                 top * (dstPitch/2) + left;
            pPriv->VBuf0offset = pPriv->UBuf0offset;
            pPriv->UBuf1offset = pPriv->UBuf0offset;
            pPriv->VBuf1offset = pPriv->UBuf0offset;
        }
    } else {
        pPriv->YBuf0offset = pPriv->videoBuf0_gtt_offset << PAGE_SHIFT;
        pPriv->YBuf1offset = pPriv->videoBuf1_gtt_offset << PAGE_SHIFT;
        if (pPriv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
            pPriv->UBuf0offset = pPriv->YBuf0offset + (Y_dstpitch  * width);
            pPriv->VBuf0offset = pPriv->UBuf0offset + (dstPitch * width / 2);
            pPriv->UBuf1offset = pPriv->YBuf1offset + (Y_dstpitch  * width);
            pPriv->VBuf1offset = pPriv->UBuf1offset + (dstPitch * width / 2);
        } else {
            pPriv->UBuf0offset = pPriv->YBuf0offset + (Y_dstpitch  * height);
            pPriv->VBuf0offset = pPriv->UBuf0offset + (dstPitch * height / 2);
            pPriv->UBuf1offset = pPriv->YBuf1offset + (Y_dstpitch  * height);
            pPriv->VBuf1offset = pPriv->UBuf1offset + (dstPitch * height / 2);
        }
    }

#else /* USE_ROTATION_FUNC */
        pPriv->YBuf0offset = vaPtr->src_srf.pre_add +
            (wsbmBOOffsetHint(pPriv->vaSrf) & 0x0FFFFFFF) + top * dstPitch + left;
        pPriv->YBuf1offset = vaPtr->src_srf.pre_add +
            (wsbmBOOffsetHint(pPriv->vaSrf) & 0x0FFFFFFF) + top * dstPitch + left;
        pPriv->UBuf0offset = pPriv->YBuf0offset + (Y_dstpitch  * height);
        pPriv->VBuf0offset = pPriv->UBuf0offset + (dstPitch * height / 2);
        pPriv->UBuf1offset = pPriv->YBuf1offset + (Y_dstpitch  * height);
        pPriv->VBuf1offset = pPriv->UBuf1offset + (dstPitch * height / 2);
        pPriv->UBuf0offset = vaPtr->src_srf.pre_add +
            (wsbmBOOffsetHint(pPriv->vaSrf) & 0x0FFFFFFF) +
            (Y_dstpitch  * height) + top * (dstPitch/2) + left;
        pPriv->VBuf0offset = pPriv->UBuf0offset;
        pPriv->UBuf1offset = pPriv->UBuf0offset;
        pPriv->VBuf1offset = pPriv->UBuf0offset;
#endif /* USE_ROTATION_FUNC */

#if 0
    switch (id) {
    case FOURCC_XVVA:
        if ((pPriv->rotation != RR_Rotate_0) && \
            (vaPtr->src_srf.pl_flags & DRM_PSB_FLAG_MEM_RAR)) {
            if (vaPtr->dst_srf.fourcc == VA_FOURCC_RGBA) {
                /*
                 *Only rotation+RAR will go into this path
                 *psbTransferVASurface rotate the src RAR buffer to dst RAR buffer
                 *the video format in dst RAR is RGBA
                 *normal rotation can also go this way, but this way need extra buffer copy
                 *so for performance issue, normal rotation use texture video adaptor for default
                 */
                ret = psbConvertNV12SurfaceToRGBA(pScrn, pPriv, vaPtr,
                                        width, height, dstPitch);
            } else {
                unsigned int dst_w, dst_h;
                dst_w = src_w;
                dst_h = src_h;
                        ret = psbBlitNV12VideoSurface(pScrn, pPriv, vaPtr,
                                width, height,
                                src_w, src_h, dst_w, dst_h, dstPitch);
            }
            if (ret != TRUE) {
                xf86DrvMsg(pScreen->myNum, X_ERROR, "%s: rotate protected video error\n", __FUNCTION__);
                return -1;
            }
        }
        break;
    case FOURCC_NV12:
        nlines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;
        I830CopyPlanarNV12Data(pScrn, pPriv, buf, srcPitch, dstPitch,
                                top, left, nlines, npixels);
        break;
    case FOURCC_YV12:
    case FOURCC_I420:
        top &= ~1;
        nlines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;
        I830CopyPlanarData(pScrn, pPriv, buf, srcPitch, srcPitch2, dstPitch,
                            height, top, left, nlines, npixels, id);
        break;
#if USE_DISPLAY_C_SPRITE
    case FOURCC_RGBA:
        nlines = ((y2 + 0xffff) >> 16) - top;
        I830CopyRGBAData(pScrn, pPriv, buf, srcPitch, dstPitch, top, left,
            nlines, npixels);
        sprite_offset = pPriv->videoBuf0_gtt_offset << PAGE_SHIFT;
        break;
#endif
    case FOURCC_UYVY:
    case FOURCC_YUY2:
        nlines = ((y2 + 0xffff) >> 16) - top;
        I830CopyPackedData(pScrn, pPriv, buf, srcPitch, dstPitch, top, left,
                            nlines, npixels);
        break;
    default:
        break;
    }

    /* update cliplist */
    if (!RegionsEqual(&pPriv->clip, clipRegion)) {
        REGION_COPY(pScrn->pScreen, &pPriv->clip, clipRegion);
        xf86XVFillKeyHelperDrawable(pDraw, pPriv->colorKey, clipRegion);
        /*i830_fill_colorkey (pScreen, pPriv->colorKey, clipBoxes);*/
    }
#endif

#if USE_DISPLAY_C_SPRITE
    if (id == FOURCC_RGBA   \
            || (id == FOURCC_XVVA   \
                    && (pPriv->rotation != RR_Rotate_0) \
                    && (vaPtr->dst_srf.fourcc == VA_FOURCC_RGBA)))
        i830_display_video_sprite(pScrn, crtc, width, height, dstPitch,
                            &dstBox, sprite_offset);
    else
#endif
    i830_display_video(ctx, pPriv, surface, destId, width, height, dstPitch, srcPitch,
                       x1, y1, x2, y2, &dstBox, src_w, src_h,
                       drw_w, drw_h, vaPtr->flags);

// FIXME : do I use two buffers here really?
//    pPriv->curBuf = (pPriv->curBuf + 1) & 1;

    return Success;
}


int
psbInitVideo(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    PsbPortPrivPtr pPriv;

    pPriv = psbSetupImageVideoOverlay(ctx);
    if (pPriv) {
        psbMoveInVideo(pPriv);
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
    if (pPriv) {
        psbMoveOutVideo(ctx, pPriv);
        psbPortPrivDestroy(ctx, pPriv);
    }
    return 0;
}


/*
 * Calculates the coefficintes of a YUV->RGB conversion based on
 * the provided basis coefficients (already had HUe and Satu applied).
 * Performs brightness and contrast adjustment as well as the required
 * offsets to put into correct range for hardware conversion.
 */
static void
psb_create_coeffs(double yOff, double uOff, double vOff, double rgbOff,
                  double yScale, double uScale, double vScale,
                  double brightness, double contrast,
                  double *pYCoeff, double *pUCoeff, double *pVCoeff,
                  double *pConstant)
{
    *pYCoeff = yScale * contrast;
    *pUCoeff = uScale * contrast;
    *pVCoeff = vScale * contrast;

    *pConstant = (((yOff + brightness) * yScale)
                  + (uOff * uScale) + (vOff * vScale)) * contrast + rgbOff;
}

/**
 * Checks if the specified coefficients are within the ranges required
 * and returns true if they are else false.
 */
static int
psb_check_coeffs(double Ycoeff, double Ucoeff, double Vcoeff,
                 double ConstantTerm, signed char byShift)
{
    if ((Ycoeff > 127) || (Ycoeff < -128)) {
        return 1;
    }
    if ((Ucoeff > 127) || (Ucoeff < -128)) {
        return 1;
    }
    if ((Vcoeff > 127) || (Vcoeff < -128)) {
        return 1;
    }
    if ((ConstantTerm > 32766) || (ConstantTerm < -32767)) {
        return 1;
    }
    return 0;
}

/*
 * Converts a floating point function in the form
 *    a*yCoeff + b*uCoeff + c * vCoeff + d
 *  Into a fixed point function of the forrm
 *   (a*pY + b * pU + c * pV + constant)>>pShift
 */
static void
psb_convert_coeffs(double Ycoeff, double Ucoeff, double Vcoeff,
                   double ConstantTerm, signed char *pY, signed char *pU,
                   signed char *pV, signed short *constant,
                   unsigned char *pShift)
{
    *pShift = 0;

    Ycoeff *= 256;
    Ucoeff *= 256;
    Vcoeff *= 256;
    ConstantTerm *= 256;
    *pShift = 8;

    /*
     * What we want to do is scale up the coefficients so that they just fit into their
     * allowed bits, so we are using signed maths giving us coefficients can be between +-128.
     * The constant can be between =- 32767.
     * The divide can be between 0 and 256 (on powers of two only).
     * A mathematical approach would be nice, but for simplicity do an iterative compare
     * and divide. Until something fits.
     */
    while (psb_check_coeffs(Ycoeff, Ucoeff, Vcoeff, ConstantTerm, *pShift)) {
        Ycoeff /= 2;
        Ucoeff /= 2;
        Vcoeff /= 2;
        ConstantTerm /= 2;
        (*pShift)--;
    }
    *pY = (signed char)(Ycoeff + 0.5);
    *pU = (signed char)(Ucoeff + 0.5);
    *pV = (signed char)(Vcoeff + 0.5);
    *constant = (signed short)(ConstantTerm + 0.5);
}

/**
 * Performs a hue and saturation adjustment on the CSC coefficients supplied.
 */
static void
psb_transform_sathuecoeffs(psb_transform_coeffs * dest,
                           const psb_transform_coeffs * const source,
                           double fHue, double fSat)
{
    double fHueSatSin, fHueSatCos;

    fHueSatSin = sin(fHue) * fSat;
    fHueSatCos = cos(fHue) * fSat;

    dest->rY = source->rY;
    dest->rCb = source->rCb * fHueSatCos - source->rCr * fHueSatSin;
    dest->rCr = source->rCr * fHueSatCos + source->rCb * fHueSatSin;

    dest->gY = source->gY;
    dest->gCb = source->gCb * fHueSatCos - source->gCr * fHueSatSin;
    dest->gCr = source->gCr * fHueSatCos + source->gCb * fHueSatSin;

    dest->bY = source->bY;
    dest->bCb = source->bCb * fHueSatCos - source->bCr * fHueSatSin;
    dest->bCr = source->bCr * fHueSatCos + source->bCb * fHueSatSin;
}

/*
 * Scales the tranfer matrix depending on the input/output
 * nominal ranges.
 */
static void
psb_scale_transfermatrix(psb_transform_coeffs * transfer_matrix,
                         double YColumScale, double CbColumScale,
                         double CrColumnScale)
{
    /* First column of the transfer matrix */
    transfer_matrix->rY *= YColumScale;
    transfer_matrix->gY *= YColumScale;
    transfer_matrix->bY *= YColumScale;

    /* Second column of the transfer matrix */
    transfer_matrix->rCb *= CbColumScale;
    transfer_matrix->gCb *= CbColumScale;
    transfer_matrix->bCb *= CbColumScale;

    /* Third column of the transfer matrix */
    transfer_matrix->rCr *= CrColumnScale;
    transfer_matrix->gCr *= CrColumnScale;
    transfer_matrix->bCr *= CrColumnScale;
}

/*
 * ITU-R BT.601, BT.709 and SMPTE 240M transfer matrices from DXVA 2.0
 * Video Color Field definitions Design Spec(Version 0.03).
 * [R', G', B'] values are in the range [0, 1], Y' is in the range [0,1]
 * and [Pb, Pr] components are in the range [-0.5, 0.5].
 */
static psb_transform_coeffs s601 = {
    1, -0.000001, 1.402,
    1, -0.344136, -0.714136,
    1, 1.772, 0
};

static psb_transform_coeffs s709 = {
    1, 0, 1.5748,
    1, -0.187324, -0.468124,
    1, 1.8556, 0
};

static psb_transform_coeffs s240M = {
    1, -0.000657, 1.575848,
    1, -0.226418, -0.476529,
    1, 1.825958, 0.000378
};

/*
  These are the corresponding matrices when using NominalRange_16_235
  for the input surface and NominalRange_0_255 for the outpur surface:

  static const psb_transform_coeffs s601 = {
  1.164,                0,              1.596,
  1.164,                -0.391,         -0.813,
  1.164,                2.018,          0
  };

  static const psb_transform_coeffs s709 = {
  1.164,                0,              1.793,
  1.164,                -0.213,         -0.534,
  1.164,                2.115,          0
  };

  static const psb_transform_coeffs s240M = {
  1.164,                -0.0007,        1.793,
  1.164,                -0.257,         -0.542,
  1.164,                2.078,          0.0004
  };
*/

/**
 * Select which transfer matrix to use in the YUV->RGB conversion.
 */
static void
psb_select_transfermatrix(PsbPortPrivRec * pPriv,
                          psb_transform_coeffs * transfer_matrix,
                          double *Y_offset, double *CbCr_offset,
                          double *RGB_offset)
{
    double RGB_scale, Y_scale, Cb_scale, Cr_scale;

    /*
     * Depending on the nominal ranges of the input YUV surface and the output RGB
     * surface, it might be needed to perform some scaling on the transfer matrix.
     * The excursion in the YUV values implies that the first column of the matrix
     * must be divided by the Y excursion, and the second and third columns be
     * divided by the U and V excursions respectively. The offset does not affect
     * the values of the matrix.
     * The excursion in the RGB values implies that all the values in the transfer
     * matrix must be multiplied by the value of the excursion.
     *
     * Example: Conversion of the SMPTE 240M transfer matrix.
     *
     * Conversion from [Y', Pb, Pr] to [R', G', B'] in the range of [0, 1]. Y' is in
     * the range of [0, 1]      and Pb and Pr in the range of [-0.5, 0.5].
     *
     * R'               1       -0.000657       1.575848                Y'
     * G'       =       1       -0.226418       -0.476529       *       Pb
     * B'               1       1.825958        0.000378                Pr
     *
     * Conversion from [Y', Cb, Cr] to {R', G', B'] in the range of [0, 1]. Y' has an
     * excursion of 219 and an offset of +16, and CB and CR have excursions of +/-112
     * and offset of +128, for a range of 16 through 240 inclusive.
     *
     * R'               1/219   -0.000657/224   1.575848/224            Y'       16
     * G'       =       1/219   -0.226418/224   -0.476529/224   *       Cb - 128
     * B'               1/219   1.825958/224    0.000378/224            Cr   128
     *
     * Conversion from [Y', Cb, Cr] to R'G'B' in the range [0, 255].
     *
     * R'                         1/219 -0.000657/224 1.575848/224                      Y'       16
     * G'       =       255 * 1/219     -0.226418/224 -0.476529/224             *       Cb - 128
     * B'                         1/219 1.825958/224  0.000378/224                      Cr   128
     */

    switch (pPriv->src_nominalrange) {
    case PSB_NominalRange_0_255:
        /* Y has a range of [0, 255], U and V have a range of [0, 255] */
        {
            double tmp = 0.0;

            (void)tmp;
        }                              /* workaroud for float point bug? */
        Y_scale = 255.0;
        *Y_offset = 0;
        Cb_scale = Cr_scale = 255;
        *CbCr_offset = 128;
        break;
    case PSB_NominalRange_16_235:
    case PSB_NominalRange_Unknown:
        /* Y has a range of [16, 235] and Cb, Cr have a range of [16, 240] */
        Y_scale = 219;
        *Y_offset = 16;
        Cb_scale = Cr_scale = 224;
        *CbCr_offset = 128;
        break;
    case PSB_NominalRange_48_208:
        /* Y has a range of [48, 208] and Cb, Cr have a range of [48, 208] */
        Y_scale = 160;
        *Y_offset = 48;
        Cb_scale = Cr_scale = 160;
        *CbCr_offset = 128;
        break;

    default:
        /* Y has a range of [0, 1], U and V have a range of [-0.5, 0.5] */
        Y_scale = 1;
        *Y_offset = 0;
        Cb_scale = Cr_scale = 1;
        *CbCr_offset = 0;
        break;
    }

    /*
     * 8-bit computer RGB,      also known as sRGB or "full-scale" RGB, and studio
     * video RGB, or "RGB with  head-room and toe-room." These are defined as follows:
     *
     * - Computer RGB uses 8 bits for each sample of red, green, and blue. Black
     * is represented by R = G = B = 0, and white is represented by R = G = B = 255.
     * - Studio video RGB uses some number of bits N for each sample of red, green,
     * and blue, where N is 8 or more. Studio video RGB uses a different scaling
     * factor than computer RGB, and it has an offset. Black is represented by
     * R = G = B = 16*2^(N-8), and white is represented by R = G = B = 235*2^(N-8).
     * However, actual values may fall outside this range.
     */
    switch (pPriv->dst_nominalrange) {
    case PSB_NominalRange_0_255:      // for sRGB
    case PSB_NominalRange_Unknown:
        /* R, G and B have a range of [0, 255] */
        RGB_scale = 255;
        *RGB_offset = 0;
        break;
    case PSB_NominalRange_16_235:     // for stRGB
        /* R, G and B have a range of [16, 235] */
        RGB_scale = 219;
        *RGB_offset = 16;
        break;
    case PSB_NominalRange_48_208:     // for Bt.1361 RGB
        /* R, G and B have a range of [48, 208] */
        RGB_scale = 160;
        *RGB_offset = 48;
        break;
    default:
        /* R, G and B have a range of [0, 1] */
        RGB_scale = 1;
        *RGB_offset = 0;
        break;
    }

    switch (pPriv->video_transfermatrix) {
    case PSB_VideoTransferMatrix_BT709:
        memcpy(transfer_matrix, &s709, sizeof(psb_transform_coeffs));
        break;
    case PSB_VideoTransferMatrix_BT601:
        memcpy(transfer_matrix, &s601, sizeof(psb_transform_coeffs));
        break;
    case PSB_VideoTransferMatrix_SMPTE240M:
        memcpy(transfer_matrix, &s240M, sizeof(psb_transform_coeffs));
        break;
    case PSB_VideoTransferMatrix_Unknown:
        /*
         * Specifies that the video transfer matrix is not specified.
         * The default value is BT601 for standard definition (SD) video and BT709
         * for high definition (HD) video.
         */
        if (1 /*pPriv->sVideoDesc.SampleWidth < 720 */ ) {      /* TODO, width selection */
            memcpy(transfer_matrix, &s601, sizeof(psb_transform_coeffs));
        } else {
            memcpy(transfer_matrix, &s709, sizeof(psb_transform_coeffs));
        }
        break;
    default:
        break;
    }

    if (Y_scale != 1 || Cb_scale != 1 || Cr_scale != 1) {
        /* Each column of the transfer matrix has to
         * be scaled by the excursion of each component
         */
        psb_scale_transfermatrix(transfer_matrix, 1 / Y_scale, 1 / Cb_scale,
                                 1 / Cr_scale);
    }
    if (RGB_scale != 1) {
        /* All the values in the transfer matrix have to be multiplied
         * by the excursion of the RGB components
         */
        psb_scale_transfermatrix(transfer_matrix, RGB_scale, RGB_scale,
                                 RGB_scale);
    }
}


/**
 * Updates the CSC coefficients if required.
 */
static void
psb_setup_coeffs(PsbPortPrivRec * pPriv)
{
    double yCoeff, uCoeff, vCoeff, Constant;
    double fContrast;
    double Y_offset, CbCr_offset, RGB_offset;
    int bright_off = 0;
    psb_transform_coeffs coeffs, transfer_matrix;

    /* Offsets in the input and output ranges are
     * included in the constant of the transform equation
     */
    psb_select_transfermatrix(pPriv, &transfer_matrix,
                              &Y_offset, &CbCr_offset, &RGB_offset);

    /*
     * It is at this point we should adjust the parameters for the procamp:
     * - Brightness is handled as an offset of the Y parameter.
     * - Contrast is an adjustment of the Y scale.
     * - Saturation is a scaling of the U anc V parameters.
     * - Hue is a rotation of the U and V parameters.
     */

    bright_off = pPriv->brightness.Value;
    fContrast = (pPriv->contrast.Value + 100) / 100.0;

    /* Apply hue and saturation correction to transfer matrix */
    psb_transform_sathuecoeffs(&coeffs,
                               &transfer_matrix,
                               pPriv->hue.Value * Degree,
                               pPriv->saturation.Value / 100.0);

    /* Create coefficients to get component R
     * (including brightness and contrast correction)
     */
    psb_create_coeffs(-1 * Y_offset, -1 * CbCr_offset, -1 * CbCr_offset,
                      RGB_offset, coeffs.rY, coeffs.rCb, coeffs.rCr,
                      bright_off, fContrast, &yCoeff, &uCoeff, &vCoeff,
                      &Constant);

    /* Convert transform operation from floating point to fixed point */
    psb_convert_coeffs(yCoeff, uCoeff, vCoeff, Constant,        /* input coefficients */
                       &pPriv->coeffs.rY, &pPriv->coeffs.rU,
                       &pPriv->coeffs.rV, &pPriv->coeffs.rConst,
                       &pPriv->coeffs.rShift);

    /* Create coefficients to get component G
     * (including brightness and contrast correction)
     */
    psb_create_coeffs(-1 * Y_offset, -1 * CbCr_offset, -1 * CbCr_offset,
                      RGB_offset, coeffs.gY, coeffs.gCb, coeffs.gCr,
                      bright_off, fContrast, &yCoeff, &uCoeff, &vCoeff,
                      &Constant);

    /* Convert transform operation from floating point to fixed point */
    psb_convert_coeffs(yCoeff, uCoeff, vCoeff, Constant,
                       /* tranfer matrix coefficients for G */
                       &pPriv->coeffs.gY, &pPriv->coeffs.gU,
                       &pPriv->coeffs.gV, &pPriv->coeffs.gConst,
                       &pPriv->coeffs.gShift);

    /* Create coefficients to get component B
     * (including brightness and contrast correction)
     */
    psb_create_coeffs(-1 * Y_offset, -1 * CbCr_offset, -1 * CbCr_offset,
                      RGB_offset, coeffs.bY, coeffs.bCb, coeffs.bCr,
                      bright_off, fContrast, &yCoeff, &uCoeff, &vCoeff,
                      &Constant);

    /* Convert transform operation from floating point to fixed point */
    psb_convert_coeffs(yCoeff, uCoeff, vCoeff, Constant,
                       /* tranfer matrix coefficients for B */
                       &pPriv->coeffs.bY, &pPriv->coeffs.bU,
                       &pPriv->coeffs.bV, &pPriv->coeffs.bConst,
                       &pPriv->coeffs.bShift);
}

static void
psb_pack_coeffs(PsbPortPrivRec * pPriv, unsigned long *sgx_coeffs)
{
    /* We use taps 0,3 and 6 which I means an filter offset of either 4,5,6
     * yyyyuvuv
     * x00x00x0
     */
    sgx_coeffs[0] =
        ((pPriv->coeffs.rY & 0xff) << 24) | ((pPriv->coeffs.rU & 0xff) << 0);
    sgx_coeffs[1] = (pPriv->coeffs.rV & 0xff) << 8;
    sgx_coeffs[2] =
        ((pPriv->coeffs.rConst & 0xffff) << 4) | (pPriv->coeffs.rShift & 0xf);

    sgx_coeffs[3] =
        ((pPriv->coeffs.gY & 0xff) << 24) | ((pPriv->coeffs.gU & 0xff) << 0);
    sgx_coeffs[4] = (pPriv->coeffs.gV & 0xff) << 8;
    sgx_coeffs[5] =
        ((pPriv->coeffs.gConst & 0xffff) << 4) | (pPriv->coeffs.gShift & 0xf);

    sgx_coeffs[6] =
        ((pPriv->coeffs.bY & 0xff) << 24) | ((pPriv->coeffs.bU & 0xff) << 0);
    sgx_coeffs[7] = (pPriv->coeffs.bV & 0xff) << 8;
    sgx_coeffs[8] =
        ((pPriv->coeffs.bConst & 0xffff) << 4) | (pPriv->coeffs.bShift & 0xf);
}


