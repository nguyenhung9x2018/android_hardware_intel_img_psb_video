/*
** psb_texture.c
** Login : <brady@luna.bj.intel.com>
** Started on  Wed Mar 31 14:40:46 2010 brady
** $Id$
** 
** Copyright (C) 2010 brady
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdio.h>
#include <math.h>

#include <psb_drm.h>
#include <va/va_backend.h>

#include <wsbm/wsbm_manager.h>

#include <pvr2d.h>
#include <pvr_android.h>

#include "psb_drv_video.h"
#include "psb_output.h"

#include "psb_texture.h"

#define LOG_TAG "psb_texture"
#include <cutils/log.h>

#define INIT_DRIVER_DATA	psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;

#define Degree (2*PI / 360.0)
#define PI 3.1415927

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

struct psb_texture_s {
	PVR2DCONTEXTHANDLE hPVR2DContext;

	struct _WsbmBufferObject *vaSrf;

	unsigned int video_transfermatrix;
	unsigned int src_nominalrange;
	unsigned int dst_nominalrange;

	uint32_t gamma0;
	uint32_t gamma1;
	uint32_t gamma2;
	uint32_t gamma3;
	uint32_t gamma4;
	uint32_t gamma5;

	ov_psb_fixed32 brightness;
	ov_psb_fixed32 contrast;
	ov_psb_fixed32 saturation;
	ov_psb_fixed32 hue;

	psb_coeffs_s coeffs;
};

typedef struct _psb_transform_coeffs_
{
    double rY, rCb, rCr;
    double gY, gCb, gCr;
    double bY, bCb, bCr;
} psb_transform_coeffs;

typedef enum _psb_videotransfermatrix
{
    PSB_VideoTransferMatrixMask = 0x07,
    PSB_VideoTransferMatrix_Unknown = 0,
    PSB_VideoTransferMatrix_BT709 = 1,
    PSB_VideoTransferMatrix_BT601 = 2,
    PSB_VideoTransferMatrix_SMPTE240M = 3
} psb_videotransfermatrix;

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

static void psb_setup_coeffs(struct psb_texture_s * pPriv);
static void psb_scale_transfermatrix(psb_transform_coeffs * transfer_matrix,
				     double YColumScale, double CbColumScale,
				     double CrColumnScale);
static void psb_select_transfermatrix(struct psb_texture_s * pPriv,
				      psb_transform_coeffs * transfer_matrix,
				      double *Y_offset, double *CbCr_offset,
				      double *RGB_offset);
static void psb_create_coeffs(double yOff, double uOff, double vOff, double rgbOff,
			      double yScale, double uScale, double vScale,
			      double brightness, double contrast,
			      double *pYCoeff, double *pUCoeff, double *pVCoeff,
			      double *pConstant);
static void psb_convert_coeffs(double Ycoeff, double Ucoeff, double Vcoeff,
			       double ConstantTerm, signed char *pY, signed char *pU,
			       signed char *pV, signed short *constant,
			       unsigned char *pShift);
static int psb_check_coeffs(double Ycoeff, double Ucoeff, double Vcoeff,
			    double ConstantTerm, signed char byShift);
static void
psb_transform_sathuecoeffs(psb_transform_coeffs * dest,
			   const psb_transform_coeffs * const source,
			   double fHue, double fSat);

unsigned long PVRCalculateStride(unsigned long widthInPixels, unsigned int bitsPerPixel)
{
#define STRIDE_ALIGNMENT		32
	// Round up to next 32 pixel boundry, as according to PVR2D docs
	int ulActiveLinelenInPixels = (widthInPixels + (STRIDE_ALIGNMENT-1)) & ~(STRIDE_ALIGNMENT-1); 
	return ((ulActiveLinelenInPixels * bitsPerPixel)+7)>>3; // pixels to bytes
}

void init_test_texture(VADriverContextP ctx)
{
	INIT_DRIVER_DATA;

	struct psb_texture_s *texture_priv;
	int ret;

	texture_priv = calloc(1, sizeof(*texture_priv));
	if (!texture_priv)
		printf("ERROR -- failed to allocate texture_priv\n");

	ret = pvr_android_context_create(&texture_priv->hPVR2DContext);
	if (ret < 0) {
		printf("%s(): null PVR context!!", __func__);
	}

	texture_priv->video_transfermatrix = PSB_VideoTransferMatrix_BT709;
	texture_priv->src_nominalrange = PSB_NominalRange_0_255;
	texture_priv->dst_nominalrange = PSB_NominalRange_0_255;

	texture_priv->brightness.Value = OV_BRIGHTNESS_DEFAULT_VALUE;
	texture_priv->brightness.Fraction = 0;
	texture_priv->contrast.Value = OV_CONTRAST_DEFAULT_VALUE;
	texture_priv->contrast.Fraction = 0;
	texture_priv->hue.Value = OV_HUE_DEFAULT_VALUE;
	texture_priv->hue.Fraction = 0;
	texture_priv->saturation.Value = OV_SATURATION_DEFAULT_VALUE;
	texture_priv->saturation.Fraction = 0;

	printf("FIXME: should has some way to modify these values\n");
	texture_priv->brightness.Value = -19; /* (255/219) * -16 */
	texture_priv->contrast.Value = 75;  /* 255/219 * 64 */
	texture_priv->saturation.Value = 146; /* 128/112 * 128 */

	texture_priv->gamma5 = 0xc0c0c0;
	texture_priv->gamma4 = 0x808080;
	texture_priv->gamma3 = 0x404040;
	texture_priv->gamma2 = 0x202020;
	texture_priv->gamma1 = 0x101010;
	texture_priv->gamma0 = 0x080808;

	driver_data->dri_priv = texture_priv;

	psb_setup_coeffs(texture_priv);
}

void deinit_test_texture(VADriverContextP ctx)
{
	INIT_DRIVER_DATA;
	struct psb_texture_s *texture_priv;

	texture_priv = (struct psb_overlay_s *)(driver_data->dri_priv);

	free(texture_priv);
	driver_data->dri_priv = NULL;
}

void blit_texture_to_buf(VADriverContextP ctx, unsigned char * data, int src_x, int src_y, int src_w,
			 int src_h, int dst_x, int dst_y, int dst_w, int dst_h,
			 int width, int height, int src_pitch, struct _WsbmBufferObject * src_buf,
			 unsigned int placement)
{
#if 0
	INIT_DRIVER_DATA;
	struct psb_texture_s *texture_priv;

	int update_coeffs = 0;
	PVR2D_VPBLT sBltVP;
	int i;
	unsigned char * tmp_buffer;
	PVR2DERROR ePVR2DStatus;
	PPVR2DMEMINFO pVaVideoMemInfo;
	PPVR2DMEMINFO pDstMeminfo;
	unsigned char temp;

	texture_priv = (struct psb_overlay_s *)(driver_data->dri_priv);

	src_pitch = (src_pitch + 0x3) & ~0x3;

	//printf("check whether we need to update coeffs\n");
	/* check whether we need to update coeffs */
	if ((height > 576) &&
	    (texture_priv->video_transfermatrix != PSB_VideoTransferMatrix_BT709)) {
		texture_priv->video_transfermatrix = PSB_VideoTransferMatrix_BT709;
		update_coeffs = 1;
	} else if ((height <= 576) &&
            (texture_priv->video_transfermatrix != PSB_VideoTransferMatrix_BT601)) {
		texture_priv->video_transfermatrix = PSB_VideoTransferMatrix_BT601;
		update_coeffs = 1;
        }

	//printf("prepare coeffs if needed\n");
	/* prepare coeffs if needed */
	memset(&sBltVP, 0, sizeof(PVR2D_VPBLT));
	if (update_coeffs == 1) {
		psb_setup_coeffs(texture_priv);
		sBltVP.psYUVCoeffs = (PPVR2D_YUVCOEFFS) &texture_priv->coeffs;
		/* FIXME: is it right? */
		sBltVP.bCoeffsGiven   = 1;
	}
	printf("now wrap the source wsbmBO\n");
	/* now wrap the source wsbmBO */
	tmp_buffer = NULL;
	tmp_buffer = wsbmBOMap (src_buf, WSBM_ACCESS_READ | WSBM_ACCESS_WRITE);
	for (i = 0; i < height * src_pitch * 1.5; i = i + 4096)
                memcpy(&temp, tmp_buffer + i, 1);
	ePVR2DStatus = PVR2DMemWrap(texture_priv->hPVR2DContext,
				    tmp_buffer,
				    0,
				    (src_pitch * height * 1.5),
				    NULL,
				    &pVaVideoMemInfo);

	/* wrap the dest source */
	/* FIXME: this is wrap for rgb565 */
	ePVR2DStatus = PVR2DMemWrap(texture_priv->hPVR2DContext,
				    data,
				    0,
				    (dst_w * dst_h * 2),
				    NULL,
				    &pDstMeminfo);
	sBltVP.sDst.pSurfMemInfo = pDstMeminfo;
	sBltVP.sDst.SurfOffset   = 0;
	/* FIXME: this wrong, how to get system pitch */
	sBltVP.sDst.Stride       = dst_w * 2;//align_to(dst_w, 64);
//	sBltVP.sDst.Stride       = PVRCalculateStride(dst_w, 16);
	sBltVP.sDst.Format       = PVR2D_RGB565;
	sBltVP.sDst.SurfWidth    = dst_w;
	sBltVP.sDst.SurfHeight   = dst_h;
     
	/* Y plane UV plane */       
	sBltVP.uiNumLayers      = 1; 
	sBltVP.sSrc->Stride     = src_pitch;
	sBltVP.sSrc->Format     = FOURCC_NV12;
	sBltVP.sSrc->SurfWidth  = width;
	sBltVP.sSrc->SurfHeight = height;
	sBltVP.sSrc[0].pSurfMemInfo = pVaVideoMemInfo;

	/* FIXME: check for top-bottom */
        sBltVP.sSrc->SurfOffset = 0;

	/* FIXME: check rotation setting */
	/* FIXME: use PVR define */
	sBltVP.RotationValue = 1;

	/* clip box */
	sBltVP.rcDest.left = dst_x;
	sBltVP.rcDest.right = dst_x + dst_w;
	sBltVP.rcDest.top = dst_y;
	sBltVP.rcDest.bottom = dst_y + dst_h;

	sBltVP.rcSource->left = src_x;
	sBltVP.rcSource->right = src_x + src_w;
	sBltVP.rcSource->top = src_y;
	sBltVP.rcSource->bottom = src_y + src_h;
	ePVR2DStatus = PVR2DBltVideo(texture_priv->hPVR2DContext, &sBltVP);
	if (ePVR2DStatus != PVR2D_OK) 
	{
	        LOGE("%s: failed to do PVR2DBltVideo with error code %d\n", 
	               __FUNCTION__, ePVR2DStatus);
                return FALSE;
	}
	ePVR2DStatus = PVR2DQueryBlitsComplete(texture_priv->hPVR2DContext, pDstMeminfo, 1);
	if (ePVR2DStatus!= PVR2D_OK)
	{
		LOGE("%s: PVR2DQueryBlitsComplete error %d\n", __FUNCTION__, ePVR2DStatus);
	}
	ePVR2DStatus = PVR2DMemFree(texture_priv->hPVR2DContext, pVaVideoMemInfo);
	ePVR2DStatus = PVR2DMemFree(texture_priv->hPVR2DContext, pDstMeminfo);
        wsbmBOUnmap(src_buf);
#endif
}

static void
psb_setup_coeffs(struct psb_texture_s * pPriv)
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
    psb_convert_coeffs(yCoeff, uCoeff, vCoeff, Constant,	/* input coefficients */
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

/*
  These are the corresponding matrices when using NominalRange_16_235
  for the input surface and NominalRange_0_255 for the outpur surface:

  static const psb_transform_coeffs s601 = {
  1.164,		0,		1.596,
  1.164,		-0.391,		-0.813,
  1.164,		2.018,		0
  };

  static const psb_transform_coeffs s709 = {
  1.164,		0,		1.793,
  1.164,		-0.213,		-0.534,
  1.164,		2.115,		0
  };

  static const psb_transform_coeffs s240M = {
  1.164,		-0.0007,	1.793,
  1.164,		-0.257,		-0.542,
  1.164,		2.078,		0.0004
  };
*/

/**
 * Select which transfer matrix to use in the YUV->RGB conversion.
 */
static void
psb_select_transfermatrix(struct psb_texture_s * pPriv,
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
	}			       /* workaroud for float point bug? */
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
	if (1 /*pPriv->sVideoDesc.SampleWidth < 720 */ ) {	/* TODO, width selection */
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

