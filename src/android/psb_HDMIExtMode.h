#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "va/va.h"
#include "va/va_backend.h"
#include "xf86drm.h"
#include "xf86drmMode.h"

#define DRM_MODE_CONNECTOR_MIPI    15

typedef enum _psb_hdmi_mode {
    OFF,
    CLONE,
    EXTENDED_VIDEO,
    EXTENDED_DESKTOP,
} psb_hdmi_mode;

typedef struct _psb_extvideo_prop_s {
    psb_hdmi_mode ExtVideoMode;

    unsigned short ExtVideoMode_XRes;
    unsigned short ExtVideoMode_YRes;
    short ExtVideoMode_X_Offset;
    short ExtVideoMode_Y_Offset;
} psb_extvideo_prop_s, *psb_extvideo_prop_p;

typedef struct _psb_HDMIExt_info_s {
    drmModeRes *resources;
    /*MIPI infos*/
    uint32_t mipi_fb_id;
    /*hdmi infos*/
    uint32_t hdmi_connector_id;
    uint32_t hdmi_encoder_id;
    uint32_t hdmi_crtc_id;
    uint32_t hdmi_fb_id;
    drmModeConnection hdmi_connection;

    psb_hdmi_mode hdmi_mode;
    psb_extvideo_prop_p hdmi_extvideo_prop;
} psb_HDMIExt_info_s, *psb_HDMIExt_info_p;

VAStatus psb_HDMIExt_get_prop(unsigned short *xres, unsigned short *yres,
                              short *xoffset, short *yoffset);

psb_hdmi_mode psb_HDMIExt_get_mode();
VAStatus psb_HDMIExt_update(VADriverContextP ctx);

VAStatus psb_HDMIExt_init(VADriverContextP ctx);
VAStatus psb_HDMIExt_deinit();
