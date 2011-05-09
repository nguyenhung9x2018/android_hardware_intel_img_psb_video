#include "psb_HDMIExtMode.h"
#include "pvr2d.h"
#include "psb_drv_video.h"

/* Global variable for HDMIExt. */
psb_HDMIExt_info_p psb_HDMIExt_info;

#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData

VAStatus psb_HDMIExt_get_prop(unsigned short *xres, unsigned short *yres,
                              short *xoffset, short *yoffset)
{
    if (!psb_HDMIExt_info || !psb_HDMIExt_info->hdmi_extvideo_prop ||
            (psb_HDMIExt_info->hdmi_extvideo_prop->ExtVideoMode == OFF)) {
        psb__error_message("%s : Failed to get HDMI prop\n", __FUNCTION__);
        return VA_STATUS_ERROR_UNKNOWN;
    }
    *xres = psb_HDMIExt_info->hdmi_extvideo_prop->ExtVideoMode_XRes;
    *yres = psb_HDMIExt_info->hdmi_extvideo_prop->ExtVideoMode_YRes;
    *xoffset = 0;
    *yoffset = 0;
    return VA_STATUS_SUCCESS;
}

psb_hdmi_mode psb_HDMIExt_get_mode()
{
    if (!psb_HDMIExt_info) {
        psb__error_message("%s : Failed to get HDMI mode\n", __FUNCTION__);
        return VA_STATUS_ERROR_UNKNOWN;
    }
    return psb_HDMIExt_info->hdmi_mode;
}

VAStatus psb_HDMIExt_update(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    drmModeCrtc *hdmi_crtc = NULL;
    drmModeConnector *hdmi_connector = NULL;
    drmModeEncoder *hdmi_encoder = NULL;
    int width = 0, height = 0;
    char *strHeight = NULL;

    hdmi_connector = drmModeGetConnector(driver_data->drm_fd, psb_HDMIExt_info->hdmi_connector_id);
    if ((!hdmi_connector->encoder_id) || (hdmi_connector->connection == DRM_MODE_DISCONNECTED)) {
        psb__information_message("%s : HDMI : DISCONNECTED.\n", __FUNCTION__);
        psb_HDMIExt_info->hdmi_connection = DRM_MODE_DISCONNECTED;
    } else {
        hdmi_encoder = drmModeGetEncoder(driver_data->drm_fd, hdmi_connector->encoder_id);
        psb_HDMIExt_info->hdmi_crtc_id = hdmi_encoder->crtc_id;
        if (!psb_HDMIExt_info->hdmi_crtc_id) {
            psb__information_message("%s : hdmi_crtc_id = 0. HDMI : no crtc attached.\n", __FUNCTION__);
            psb_HDMIExt_info->hdmi_connection = DRM_MODE_DISCONNECTED;
        } else {
            psb_extvideo_prop_p hdmi_extvideo_prop = psb_HDMIExt_info->hdmi_extvideo_prop;

            psb__information_message("%s :  psb_HDMIExt_info->hdmi_crtc_id= %d.\n", __FUNCTION__,
                                     psb_HDMIExt_info->hdmi_crtc_id);
            /*Update HDMI fb id*/
            hdmi_crtc = drmModeGetCrtc(driver_data->drm_fd, psb_HDMIExt_info->hdmi_crtc_id);
            if (!hdmi_crtc) {
                /* No CRTC attached to HDMI. */
                psb__information_message("%s : Failed to get hdmi crtc. HDMI : OFF\n", __FUNCTION__);
                psb_HDMIExt_info->hdmi_connection = DRM_MODE_DISCONNECTED;
            } else {
                psb_HDMIExt_info->hdmi_connection = DRM_MODE_CONNECTED;
                strHeight = strstr(hdmi_crtc->mode.name, "x");
                hdmi_extvideo_prop->ExtVideoMode_XRes = (unsigned short)atoi(hdmi_crtc->mode.name);
                hdmi_extvideo_prop->ExtVideoMode_YRes = (unsigned short)atoi(strHeight + 1);
                psb_HDMIExt_info->hdmi_fb_id = hdmi_crtc->buffer_id;
                psb__information_message("%s : psb_HDMIExt_info->hdmi_fb_id = %d, size = %d x %d\n", __FUNCTION__,
                                         psb_HDMIExt_info->hdmi_fb_id, hdmi_extvideo_prop->ExtVideoMode_XRes, hdmi_extvideo_prop->ExtVideoMode_YRes);
                drmModeFreeCrtc(hdmi_crtc);
            }
        }
        drmModeFreeEncoder(hdmi_encoder);
    }
    drmModeFreeConnector(hdmi_connector);

    /*Update hdmi mode and hdmi ExtVideo prop*/
    if (psb_HDMIExt_info->hdmi_connection == DRM_MODE_DISCONNECTED) {
        psb_HDMIExt_info->hdmi_extvideo_prop->ExtVideoMode = OFF;
        psb_HDMIExt_info->hdmi_mode = OFF;
    } else if (psb_HDMIExt_info->hdmi_fb_id == psb_HDMIExt_info->mipi_fb_id) {
        psb_HDMIExt_info->hdmi_extvideo_prop->ExtVideoMode = CLONE;
        psb_HDMIExt_info->hdmi_mode = CLONE;
    } else {
        psb_HDMIExt_info->hdmi_extvideo_prop->ExtVideoMode = EXTENDED_VIDEO;
        psb_HDMIExt_info->hdmi_mode = EXTENDED_VIDEO;
    }
    return VA_STATUS_SUCCESS;
}

VAStatus psb_HDMIExt_init(VADriverContextP ctx)
{
    INIT_DRIVER_DATA;
    drmModeConnector *connector = NULL;
    drmModeEncoder *mipi_encoder = NULL;
    drmModeCrtc *mipi_crtc = NULL;
    int mipi_connector_id = 0, mipi_encoder_id = 0, mipi_crtc_id = 0, i;

    psb_HDMIExt_info = (psb_HDMIExt_info_p)calloc(1, sizeof(psb_HDMIExt_info_s));
    if (!psb_HDMIExt_info) {
        psb__error_message("%s : Failed to create psb_HDMIExt_info.\n", __FUNCTION__);
        return VA_STATUS_ERROR_UNKNOWN;
    }
    memset(psb_HDMIExt_info, 0, sizeof(psb_HDMIExt_info_s));

    psb_HDMIExt_info->hdmi_extvideo_prop = (psb_extvideo_prop_p)calloc(1, sizeof(psb_extvideo_prop_s));
    if (!psb_HDMIExt_info->hdmi_extvideo_prop) {
        psb__error_message("%s : Failed to create hdmi_extvideo_prop.\n", __FUNCTION__);
        return VA_STATUS_ERROR_UNKNOWN;
    }
    memset(psb_HDMIExt_info->hdmi_extvideo_prop, 0, sizeof(psb_extvideo_prop_s));

    /*Get Resources.*/
    psb_HDMIExt_info->resources = drmModeGetResources(driver_data->drm_fd);
    if (!psb_HDMIExt_info->resources) {
        psb__error_message("%s : drmModeGetResources failed.\n", __FUNCTION__);
        goto exit;
    }

    /*Get MIPI and HDMI connector id.*/
    for (i = 0; i < psb_HDMIExt_info->resources->count_connectors; i++) {
        connector = drmModeGetConnector(driver_data->drm_fd, psb_HDMIExt_info->resources->connectors[i]);

        if (!connector) {
            psb__error_message("%s : Failed to get connector %i\n", __FUNCTION__,
                               psb_HDMIExt_info->resources->connectors[i]);
            continue;
        }

        if (connector->connector_type == DRM_MODE_CONNECTOR_DVID)
            psb_HDMIExt_info->hdmi_connector_id = connector->connector_id;

        if ((connector->connector_type == /*DRM_MODE_CONNECTOR_MIPI*/15) &&
                (!mipi_connector_id)) {
            mipi_connector_id = connector->connector_id;
            mipi_encoder_id = connector->encoder_id;
        }

        drmModeFreeConnector(connector);
    }

    if (!mipi_connector_id ||
            !psb_HDMIExt_info->hdmi_connector_id ||
            !mipi_encoder_id) {
        psb__error_message("%s : Failed to get connector id or mipi encoder id. mipi_connector_id=%d, hdmi_connector_id=%d, mipi_encoder_id=%d\n", __FUNCTION__,
                           mipi_connector_id, psb_HDMIExt_info->hdmi_connector_id, mipi_encoder_id);
        goto exit;
    }

    mipi_encoder = drmModeGetEncoder(driver_data->drm_fd, mipi_encoder_id);
    if (!mipi_encoder) {
        psb__error_message("%s : Failed to get mipi encoder %i\n", __FUNCTION__);
        goto exit;
    }

    mipi_crtc_id = mipi_encoder->crtc_id;
    psb__information_message("%s : mipi_crtc_id = %d\n", __FUNCTION__,
                             mipi_crtc_id);

    drmModeFreeEncoder(mipi_encoder);

    /*Update MIPI fb id*/
    mipi_crtc = drmModeGetCrtc(driver_data->drm_fd, mipi_crtc_id);
    if (!mipi_crtc) {
        /* No CRTC attached to MIPI. */
        psb__error_message("%s : MIPI : OFF\n", __FUNCTION__);
        goto exit;
    }
    psb_HDMIExt_info->mipi_fb_id = mipi_crtc->buffer_id;
    psb__information_message("%s : psb_HDMIExt_info->mipi_fb_id = %d\n", __FUNCTION__,
                             psb_HDMIExt_info->mipi_fb_id);
    drmModeFreeCrtc(mipi_crtc);

    if (psb_HDMIExt_update(ctx))
        goto exit;

    return VA_STATUS_SUCCESS;

exit:
    if (psb_HDMIExt_info->resources)
        drmModeFreeResources(psb_HDMIExt_info->resources);

    if (connector)
        drmModeFreeConnector(connector);

    free(psb_HDMIExt_info);

    return VA_STATUS_ERROR_UNKNOWN;
}

VAStatus psb_HDMIExt_deinit()
{
    if (psb_HDMIExt_info->resources)
        drmModeFreeResources(psb_HDMIExt_info->resources);

    if (psb_HDMIExt_info->hdmi_extvideo_prop)
        free(psb_HDMIExt_info->hdmi_extvideo_prop);
    if (psb_HDMIExt_info)
        free(psb_HDMIExt_info);

    return VA_STATUS_SUCCESS;
}

