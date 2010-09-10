#ifndef _PSB_XRANDR_H_
#define _PSB_XRANDR_H_
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>	/* we share subpixel information */
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <va/va_backend.h>

typedef enum _psb_hdmi_mode {
    CLONE, 
    EXTENDED, 
    EXTENDEDVIDEO, 
    SINGLE,
    UNKNOWNVIDEOMODE,
} psb_hdmi_mode;

typedef enum _psb_extvideo_center {
    NOCENTER,
    CENTER,
    UNKNOWNCENTER,
} psb_extvideo_center;

typedef enum _psb_extvideo_subtitle {
    BOTH, 
    HDMI, 
    NOSUBTITLE,
} psb_extvideo_subtitle;

typedef enum _psb_xrandr_location
{
    NORMAL, ABOVE, BELOW, LEFT_OF, RIGHT_OF,
} psb_xrandr_location;

typedef struct _psb_extvideo_prop_s {
    psb_hdmi_mode ExtVideoMode;

    unsigned int ExtVideoMode_XRes;
    unsigned int ExtVideoMode_YRes;
    unsigned int ExtVideoMode_X_Offset;
    unsigned int ExtVideoMode_Y_Offset;

    psb_extvideo_center ExtVideoMode_Center;
    psb_extvideo_subtitle ExtVideoMode_SubTitle;

} psb_extvideo_prop_s, *psb_extvideo_prop_p;

typedef struct _psb_xrandr_crtc_s {
    struct _psb_xrandr_crtc_s *next;

    RRCrtc crtc_id;
    RRMode crtc_mode;

    unsigned int width;
    unsigned int height;
    unsigned int x;
    unsigned int y;

    psb_xrandr_location location;

    int noutput;

    struct _psb_xrandr_output_s **output;

} psb_xrandr_crtc_s, *psb_xrandr_crtc_p;

typedef struct _psb_xrandr_output_s
{
    RROutput output_id;
    char name[10];
    struct _psb_xrandr_output_s *next;

    Connection connection;

    unsigned long mm_width;
    unsigned long mm_height;

    psb_xrandr_crtc_p crtc;

} psb_xrandr_output_s, *psb_xrandr_output_p;

typedef struct _psb_xrandr_info_s
{
    psb_xrandr_crtc_p primary_crtc;
    psb_xrandr_crtc_p extend_crtc;

    psb_xrandr_output_p primary_output;
    psb_xrandr_output_p extend_output;

    unsigned int nconnected_output;

    psb_extvideo_prop_p hdmi_extvideo_prop;
    int hdmi_connected;
    int mipi1_connected;
    int output_changed;
} psb_xrandr_info_s, *psb_xrandr_info_p;


Atom psb_exit_atom;
int psb_xrandr_hdmi_connected();
int psb_xrandr_mipi1_connected();
int psb_xrandr_single_mode();
int psb_xrandr_clone_mode();
int psb_xrandr_extend_mode();
int psb_xrandr_extvideo_mode();
int psb_xrandr_outputchanged();

Window psb_xrandr_create_full_screen_window();
VAStatus psb_xrandr_extvideo_mode_prop(unsigned int *xres, unsigned int *yres, unsigned int *xoffset, unsigned int *yoffset, 
				  psb_extvideo_center *center, psb_extvideo_subtitle *subtitle);
VAStatus psb_xrandr_primary_crtc_coordinate(int *x, int *y, int *width, int *height);
VAStatus psb_xrandr_extend_crtc_coordinate(int *x, int *y, int *width, int *height, psb_xrandr_location *location);

void psb_xrandr_refresh();
void psb_xrandr_thread();
VAStatus psb_xrandr_init (VADriverContextP ctx);
VAStatus psb_xrandr_deinit(Drawable draw);

#endif /* _PSB_XRANDR_H_ */
