#include "psb_xrandr.h"
#include "psb_x11.h"

psb_xrandr_info_p psb_xrandr_info;
static psb_xrandr_crtc_p crtc_head, crtc_tail;
static psb_xrandr_output_p output_head, output_tail;
static pthread_mutex_t psb_extvideo_mutex;
static XRRScreenResources *res;
Display *dpy;
Window root;

#define MWM_HINTS_DECORATIONS (1L << 1)
typedef struct
{
        int flags;
        int functions;
        int decorations;
        int input_mode;
        int status;
} MWMHints;

char* location2string(psb_xrandr_location location)
{
    switch (location)
    {
	case ABOVE:
	    return "ABOVE";
	    break;
	case BELOW:
	    return "BELOW";
	    break;
	case LEFT_OF:
	    return "LEFT_OF";
	    break;
	case RIGHT_OF:
	    return "RIGHT_OF";
	    break;
	default:
	    return "NORMAL";
	    break;
    }
}

static psb_xrandr_output_p get_output_by_id(RROutput output_id)
{
    psb_xrandr_output_p p_output;
    for (p_output = output_head; p_output; p_output = p_output->next) {
	if (p_output->output_id == output_id)
	    return p_output;
    }
    return NULL;
}

static psb_xrandr_crtc_p get_crtc_by_id(RRCrtc crtc_id)
{
    psb_xrandr_crtc_p p_crtc;
    for (p_crtc = crtc_head; p_crtc; p_crtc = p_crtc->next)
	if (p_crtc->crtc_id == crtc_id)
	    return p_crtc;
    return NULL;
}

static void psb_extvideo_prop()
{
    psb_xrandr_crtc_p p_crtc;
    psb_xrandr_output_p p_output;
#if 0
    int i, j, nprop, actual_format;
    unsigned long nitems, bytes_after;
    Atom *props;
    Atom actual_type;
    unsigned char* prop;
    unsigned char* prop_name;
#endif
    psb_xrandr_info->output_changed = 1;
    if (psb_xrandr_info->nconnected_output == 1)
    {
	for (p_output = output_head; p_output; p_output = p_output->next)
	    if (p_output->connection == RR_Connected)
		psb_xrandr_info->primary_output = p_output;

	for (p_crtc = crtc_head; p_crtc; p_crtc = p_crtc->next)
	{
	    if (p_crtc->noutput == 0)
		continue;
	    else
		psb_xrandr_info->primary_crtc = p_crtc;
	}

	psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode = SINGLE;
    }
    else if (psb_xrandr_info->nconnected_output >= 2)
    {
	for (p_output = output_head; p_output; p_output = p_output->next)
	{
	    if (p_output->connection != RR_Connected)
		continue;

	    if (!strcmp(p_output->name, "MIPI0"))
	    {
		psb_xrandr_info->primary_output = p_output;
		psb_xrandr_info->primary_crtc = p_output->crtc;
	    }
	    else if (!strcmp(p_output->name, "MIPI1"))
	    {
                psb_xrandr_info->mipi1_connected = 1;
		psb_xrandr_info->extend_output = p_output;
		psb_xrandr_info->extend_crtc = p_output->crtc;
	    }
	    else if (!strcmp(p_output->name, "TMDS0-1"))
	    {	    
	        psb_xrandr_info->hdmi_connected = 1;
		psb_xrandr_info->extend_output = p_output;
		psb_xrandr_info->extend_crtc = p_output->crtc;
	    }
	}

	if (!psb_xrandr_info->primary_crtc || !psb_xrandr_info->extend_crtc || !psb_xrandr_info->primary_output || !psb_xrandr_info->extend_output)
	{
	    psb__error_message("Xrandr: failed to get primary/extend crtc/output\n");
	    return;
	}

	if (psb_xrandr_info->primary_crtc->x == 0 && psb_xrandr_info->primary_crtc->y == 0 \
	    && psb_xrandr_info->extend_crtc->x == 0 && psb_xrandr_info->extend_crtc->y == 0)
	    psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode = CLONE;
	else {
	    psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode = EXTENDED;

	    if (psb_xrandr_info->primary_crtc->y == psb_xrandr_info->extend_crtc->height)
		psb_xrandr_info->extend_crtc->location = ABOVE;
	    else if (psb_xrandr_info->extend_crtc->y == psb_xrandr_info->primary_crtc->height)
		psb_xrandr_info->extend_crtc->location = BELOW;
	    else if (psb_xrandr_info->primary_crtc->x == psb_xrandr_info->extend_crtc->width)
		psb_xrandr_info->extend_crtc->location = LEFT_OF;
	    else if (psb_xrandr_info->extend_crtc->x == psb_xrandr_info->primary_crtc->width)
		psb_xrandr_info->extend_crtc->location = RIGHT_OF;
	}
#if 0
	props = XRRListOutputProperties(dpy, psb_xrandr_info->extend_output->output_id, &nprop);
	for (i = 0; i < nprop; i++) {
	    XRRGetOutputProperty(dpy, psb_xrandr_info->extend_output->output_id, props[i], 
				 0, 100, False, False, AnyPropertyType, &actual_type, &actual_format,
				 &nitems, &bytes_after, &prop);

	    XRRQueryOutputProperty(dpy, psb_xrandr_info->extend_output->output_id, props[i]);
	    prop_name = XGetAtomName(dpy, props[i]);

	    if (!strcmp(prop_name, "ExtVideoMode"))
		for (j = 0; j < nitems; j++)
		    psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode = (int)((INT32*)prop)[j];
	    else if (!strcmp(prop_name, "ExtVideoMode_XRes"))
		for (j = 0; j < nitems; j++)
		    psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_XRes = (int)((INT32*)prop)[j];
	    else if (!strcmp(prop_name, "ExtVideoMode_YRes"))
		for (j = 0; j < nitems; j++)
		    psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_YRes = (int)((INT32*)prop)[j];
	    else if (!strcmp(prop_name, "ExtVideoMode_X_Offset"))
		for (j = 0; j < nitems; j++)
		    psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_X_Offset = (int)((INT32*)prop)[j];
	    else if (!strcmp(prop_name, "ExtVideoMode_Y_Offset"))
		for (j = 0; j < nitems; j++)
		    psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_Y_Offset = (int)((INT32*)prop)[j];
	    else if (!strcmp(prop_name, "ExtVideoMode_Center"))
		for (j = 0; j < nitems; j++)
		    psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_Center = (int)((INT32*)prop)[j];
	    else if (!strcmp(prop_name, "ExtVideoMode_SubTitle"))
		for (j = 0; j < nitems; j++)
		    psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_SubTitle = (int)((INT32*)prop)[j];
	}

	if (psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode == CLONE)
	    psb_xrandr_info->extend_crtc->location = NORMAL;
	else if (psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode == EXTENDED) {
	    if (psb_xrandr_info->primary_crtc->y == psb_xrandr_info->extend_crtc->height)
		psb_xrandr_info->extend_crtc->location = ABOVE;
	    else if (psb_xrandr_info->extend_crtc->y == psb_xrandr_info->primary_crtc->height)
		psb_xrandr_info->extend_crtc->location = BELOW;
	    else if (psb_xrandr_info->primary_crtc->x == psb_xrandr_info->extend_crtc->width)
		psb_xrandr_info->extend_crtc->location = LEFT_OF;
	    else if (psb_xrandr_info->extend_crtc->x == psb_xrandr_info->primary_crtc->width)
		psb_xrandr_info->extend_crtc->location = RIGHT_OF;
	}
#if 0
	else if (psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode == EXTENDEDVIDEO) {
	    if (psb_xrandr_info->primary_crtc->y == psb_xrandr_info->extend_crtc->height)
		psb_xrandr_info->extend_crtc->location = ABOVE;
	    else if (psb_xrandr_info->extend_crtc->y == psb_xrandr_info->primary_crtc->height)
		psb_xrandr_info->extend_crtc->location = BELOW;
	    else if (psb_xrandr_info->primary_crtc->x == psb_xrandr_info->extend_crtc->width)
		psb_xrandr_info->extend_crtc->location = LEFT_OF;
	    else if (psb_xrandr_info->extend_crtc->x == psb_xrandr_info->primary_crtc->width)
		psb_xrandr_info->extend_crtc->location = RIGHT_OF;
	}
#endif
#endif
    }
}

void psb_xrandr_refresh()
{
    int i, j;

    XRROutputInfo *output_info;
    XRRCrtcInfo *crtc_info;

    psb_xrandr_crtc_p p_crtc;
    psb_xrandr_output_p p_output;

    pthread_mutex_lock(&psb_extvideo_mutex);

    if (psb_xrandr_info)
    {
	if (psb_xrandr_info->hdmi_extvideo_prop)
	    free(psb_xrandr_info->hdmi_extvideo_prop);

 	free(psb_xrandr_info);

	psb_xrandr_info = NULL;
    }

    psb_xrandr_info = (psb_xrandr_info_p)calloc(1, sizeof(psb_xrandr_info_s));
    memset(psb_xrandr_info, 0, sizeof(psb_xrandr_info_s));

    psb_xrandr_info->hdmi_extvideo_prop = (psb_extvideo_prop_p)calloc(1, sizeof(psb_extvideo_prop_s));
    memset(psb_xrandr_info->hdmi_extvideo_prop, 0, sizeof(psb_extvideo_prop_s));

    //deinit crtc
    if (crtc_head)
    {
	while (crtc_head)
	{
	    crtc_tail = crtc_head->next;

	    free(crtc_head->output);
	    free(crtc_head);
	
	    crtc_head = crtc_tail;
	}
	crtc_head = crtc_tail = NULL;
    }

    for (i = 0; i < res->ncrtc; i++)
    {
	crtc_info = XRRGetCrtcInfo (dpy, res, res->crtcs[i]);
	if (crtc_info)
	{
	    p_crtc = (psb_xrandr_crtc_p)calloc(1, sizeof(psb_xrandr_crtc_s));
	    if (!p_crtc)
		psb__error_message("output of memory\n");

	    if (i == 0)
		crtc_head = crtc_tail = p_crtc;

	    p_crtc->crtc_id = res->crtcs[i];
	    p_crtc->x = crtc_info->x;
	    p_crtc->y = crtc_info->y;
	    p_crtc->width = crtc_info->width;
	    p_crtc->height = crtc_info->height;
	    p_crtc->crtc_mode = crtc_info->mode;
	    p_crtc->noutput = crtc_info->noutput;

	    crtc_tail->next = p_crtc;
	    p_crtc->next = NULL;
	    crtc_tail = p_crtc;
	}
	else{
	    psb__error_message("failed to get crtc_info\n");
            pthread_mutex_unlock(&psb_extvideo_mutex);
	    return;
	}
    }

    //deinit output
    if (output_head)
    {
	while (output_head)
	{
	    output_tail = output_head->next;

	    free(output_head);
	
	    output_head = output_tail;
	}
	output_head = output_tail = NULL;
    }

    for (i = 0; i < res->noutput; i++)
    {
	output_info = XRRGetOutputInfo(dpy, res, res->outputs[i]);
	if (output_info)
	{
	    p_output = (psb_xrandr_output_p)calloc(1, sizeof(psb_xrandr_output_s));
	    if (!output_info)
		psb__error_message("output of memory\n");

	    if (i == 0)
		output_head = output_tail = p_output;

	    p_output->output_id = res->outputs[i];

	    p_output->connection = output_info->connection;
	    if (p_output->connection == RR_Connected)
		psb_xrandr_info->nconnected_output++;

	    p_output->mm_width = output_info->mm_width;
	    p_output->mm_height = output_info->mm_height;

	    strcpy(p_output->name, output_info->name);

	    if (output_info->crtc)
		p_output->crtc = get_crtc_by_id(output_info->crtc);
	    else
		p_output->crtc = NULL;


	    output_tail->next = p_output;
	    p_output->next = NULL;
	    output_tail = p_output;
	}
	else
	{
	    psb__error_message("failed to get output_info\n");
	    pthread_mutex_unlock(&psb_extvideo_mutex);
	    return;
	}
    }

    for (p_crtc = crtc_head; p_crtc; p_crtc = p_crtc->next)
    {
	crtc_info = XRRGetCrtcInfo (dpy, res, p_crtc->crtc_id);

	p_crtc->output = (struct _psb_xrandr_output_s**)calloc(p_crtc->noutput, sizeof(psb_xrandr_output_s));

	for (j = 0; j < crtc_info->noutput; j++)
	{
	    p_output = get_output_by_id(crtc_info->outputs[j]);
	    if (p_output)
		p_crtc->output[j] = p_output;
	    else
		p_crtc->output[j] = NULL;
	}
    }

    psb_extvideo_prop();
    pthread_mutex_unlock(&psb_extvideo_mutex);
}

void psb_xrandr_thread()
{
    int event_base, error_base;
    XEvent event;
    XRRQueryExtension(dpy, &event_base, &error_base);
    XRRSelectInput(dpy, root, RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask | RROutputChangeNotifyMask | RROutputPropertyNotifyMask);

    psb__information_message("Xrandr: psb xrandr thread start\n");

    while (1)
    {
	XNextEvent(dpy, (XEvent *)&event);
	if (event.type == ClientMessage) {
	    psb__information_message("Xrandr: receive ClientMessage event, thread should exit\n");
	    XClientMessageEvent *evt;
	    evt = (XClientMessageEvent*)&event;
	    if (evt->message_type == psb_exit_atom) {
                psb__information_message("Xrandr: xrandr thread exit safely\n");
                pthread_exit(NULL);
            }
	}
	switch (event.type - event_base) {
	    case RRNotify_OutputChange:
		XRRUpdateConfiguration (&event);
		psb__information_message("Xrandr: receive RRNotify_OutputChange event, refresh output/crtc info\n");
		psb_xrandr_refresh();
		break;
	    default:
		break;
        }
    }
}

Window psb_xrandr_create_full_screen_window()
{
    int x, y, width, height;
    Window win;

    x = psb_xrandr_info->extend_crtc->x;
    y = psb_xrandr_info->extend_crtc->y;
    width = psb_xrandr_info->extend_crtc->width;
    height = psb_xrandr_info->extend_crtc->height;

    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), x, y, width, height, 0, 0, 0);

    MWMHints mwmhints;
    Atom MOTIF_WM_HINTS;

    mwmhints.flags = MWM_HINTS_DECORATIONS;
    mwmhints.decorations = 0; /* MWM_DECOR_BORDER */
    MOTIF_WM_HINTS = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    XChangeProperty(dpy, win, MOTIF_WM_HINTS, MOTIF_WM_HINTS, sizeof(long)*8,
		    PropModeReplace, (unsigned char*) &mwmhints, sizeof(mwmhints)/sizeof(long));

    XSetWindowAttributes attributes;
    attributes.override_redirect = 1;
    unsigned long valuemask;
    valuemask = CWOverrideRedirect ;
    XChangeWindowAttributes(dpy, win, valuemask, &attributes);

    XMapWindow(dpy, win);
    XFlush(dpy);
    return win;
}

#if 0
void show_current()
{
    int i, ret;
    int x, y, widht, height;
    psb_xrandr_location location;

    ret = pthread_mutex_trylock(&psb_extvideo_mutex);
    if (ret != 0)
    {
	printf("mutex busy, should not read\n");
	return;
    }

    if (psb_xrandr_single_mode())
    {
	printf("single mode\n");
	printf("primary crtc info:\n");

	ret = psb_xrandr_primary_crtc_coordinate(&x, &y, &widht, &height);

	if (!ret)
	    printf("failed to get primary crtc info\n");
	else
	{
	    printf("\tx = %d, y = %d, widht = %d, height = %d\n", x, y, widht, height);
	    printf("\tcrtc id: %08x, crtc mode: %08x, ", psb_xrandr_info->primary_crtc->crtc_id, psb_xrandr_info->primary_crtc->crtc_mode);
	    for (i = 0; i < psb_xrandr_info->primary_crtc->noutput; i++)
	    printf("output: %08x\n", psb_xrandr_info->primary_crtc->output[i]->output_id);
	}
    }
    else if (psb_xrandr_clone_mode())
    {
	printf("clone mode\n");

	ret = psb_xrandr_primary_crtc_coordinate(&x, &y, &widht, &height);

	if (!ret)
	    printf("failed to get primary crtc info\n");
	else
	{
	    printf("primary crtc info:\n");
	    printf("\tx = %d, y = %d, widht = %d, height = %d\n", x, y, widht, height);
	    printf("\tcrtc id: %08x, crtc mode: %08x, ", psb_xrandr_info->primary_crtc->crtc_id, psb_xrandr_info->primary_crtc->crtc_mode);
	    for (i = 0; i < psb_xrandr_info->primary_crtc->noutput; i++)
	    printf("output: %08x\n", psb_xrandr_info->primary_crtc->output[i]->output_id);
	}

	ret = psb_xrandr_extend_crtc_coordinate(&x, &y, &widht, &height, &location);

	if (!ret)
	    printf("failed to get clone crtc info\n");
	else
	{
	    printf("clone crtc info:\n");
	    printf("\tx = %d, y = %d, widht = %d, height = %d, location = %s\n", x, y, widht, height, location2string(location));
	    printf("\tcrtc id: %08x, crtc mode: %08x, ", psb_xrandr_info->extend_crtc->crtc_id, psb_xrandr_info->extend_crtc->crtc_mode);
	    for (i = 0; i < psb_xrandr_info->extend_crtc->noutput; i++)
	    printf("output: %08x\n", psb_xrandr_info->extend_crtc->output[i]->output_id);
	}
    }
    else if (psb_xrandr_extend_mode())
    {
	printf("extend mode\n");
	ret = psb_xrandr_primary_crtc_coordinate(&x, &y, &widht, &height);

	if (!ret)
	    printf("failed to get primary crtc info\n");
	else
	{
	    printf("primary crtc info:\n");
	    printf("\tx = %d, y = %d, widht = %d, height = %d\n", x, y, widht, height);
	    printf("\tcrtc id: %08x, crtc mode: %08x, ", psb_xrandr_info->primary_crtc->crtc_id, psb_xrandr_info->primary_crtc->crtc_mode);
	    for (i = 0; i < psb_xrandr_info->primary_crtc->noutput; i++)
	    printf("output: %08x\n", psb_xrandr_info->primary_crtc->output[i]->output_id);
	}

	ret = psb_xrandr_extend_crtc_coordinate(&x, &y, &widht, &height, &location);

	if (!ret)
	    printf("failed to get extend crtc info\n");
	else
	{
	    printf("extend crtc info:\n");
	    printf("\tx = %d, y = %d, widht = %d, height = %d, location = %s\n", x, y, widht, height, location2string(location));
	    printf("\tcrtc id: %08x, crtc mode: %08x, ", psb_xrandr_info->extend_crtc->crtc_id, psb_xrandr_info->extend_crtc->crtc_mode);
	    for (i = 0; i < psb_xrandr_info->extend_crtc->noutput; i++)
	    printf("output: %08x\n", psb_xrandr_info->extend_crtc->output[i]->output_id);
	}
    }
    else if (psb_xrandr_extvideo_mode())
	printf("extvideo mode\n");

    pthread_mutex_unlock(&psb_extvideo_mutex);
}
#endif
int psb_xrandr_hdmi_connected()
{
    int ret;
    pthread_mutex_lock(&psb_extvideo_mutex);
    ret = psb_xrandr_info->hdmi_connected;
    pthread_mutex_unlock(&psb_extvideo_mutex);
    return ret;
}

int psb_xrandr_mipi1_connected()
{
    int ret;
    pthread_mutex_lock(&psb_extvideo_mutex);
    ret = psb_xrandr_info->mipi1_connected;
    pthread_mutex_unlock(&psb_extvideo_mutex);
    return ret;
}

int psb_xrandr_single_mode()
{
    int ret;
    pthread_mutex_lock(&psb_extvideo_mutex);
    ret = (psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode == SINGLE) ? 1 : 0;
    pthread_mutex_unlock(&psb_extvideo_mutex);
    return ret;
}

int psb_xrandr_clone_mode()
{
    int ret;
    pthread_mutex_lock(&psb_extvideo_mutex);
    ret = (psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode == CLONE) ? 1 : 0;
    pthread_mutex_unlock(&psb_extvideo_mutex);
    return ret;
}

int psb_xrandr_extend_mode()
{
    int ret;
    pthread_mutex_lock(&psb_extvideo_mutex);
    ret = (psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode == EXTENDED) ? 1 : 0;
    pthread_mutex_unlock(&psb_extvideo_mutex);
    return ret;
}

int psb_xrandr_extvideo_mode()
{
    int ret;
    pthread_mutex_lock(&psb_extvideo_mutex);
    ret = (psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode == EXTENDEDVIDEO) ? 1 : 0;
    pthread_mutex_unlock(&psb_extvideo_mutex);
    return ret;
}

int psb_xrandr_outputchanged()
{
    int ret;
    pthread_mutex_lock(&psb_extvideo_mutex);
    if (psb_xrandr_info->output_changed){
        psb_xrandr_info->output_changed = 0;
        ret = 1;
    } else 
        ret = 0;
    pthread_mutex_unlock(&psb_extvideo_mutex);
    return ret;
}

VAStatus psb_xrandr_extvideo_mode_prop(unsigned int *xres, unsigned int *yres, unsigned int *xoffset, 
			      unsigned int *yoffset, psb_extvideo_center *center, psb_extvideo_subtitle *subtitle)
{
    pthread_mutex_lock(&psb_extvideo_mutex);

    if (psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode != EXTENDEDVIDEO){
        pthread_mutex_unlock(&psb_extvideo_mutex);
	return VA_STATUS_ERROR_UNKNOWN;
    }

    *xres = psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_XRes;
    *yres = psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_YRes;
    *xoffset = psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_X_Offset;
    *yoffset = psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_Y_Offset;
    *center = psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_Center;
    *subtitle = psb_xrandr_info->hdmi_extvideo_prop->ExtVideoMode_SubTitle;

    pthread_mutex_unlock(&psb_extvideo_mutex);
    return VA_STATUS_SUCCESS;
}

VAStatus psb_xrandr_primary_crtc_coordinate(int *x, int *y, int *width, int *height)
{
    pthread_mutex_lock(&psb_extvideo_mutex);
    psb_xrandr_crtc_p crtc = psb_xrandr_info->primary_crtc;;
    if (crtc) {
	*x = crtc->x;
	*y = crtc->y;
	*width = crtc->width - 1;
	*height = crtc->height - 1;	
	pthread_mutex_unlock(&psb_extvideo_mutex);
	psb__information_message("Xrandr: crtc %08x coordinate: x = %d, y = %d, widht = %d, height = %d\n",
				  psb_xrandr_info->primary_crtc->crtc_id, *x, *y, *width + 1, *height + 1);
	return VA_STATUS_SUCCESS;
    }
    pthread_mutex_unlock(&psb_extvideo_mutex);
    return VA_STATUS_ERROR_UNKNOWN;
}

VAStatus psb_xrandr_extend_crtc_coordinate(int *x, int *y, int *width, int *height, psb_xrandr_location *location)
{
    pthread_mutex_lock(&psb_extvideo_mutex);

    if (psb_xrandr_info->nconnected_output == 1 || !psb_xrandr_info->extend_crtc){	
	pthread_mutex_unlock(&psb_extvideo_mutex);
	return VA_STATUS_ERROR_UNKNOWN;
    }

    *x = psb_xrandr_info->extend_crtc->x;
    *y = psb_xrandr_info->extend_crtc->y;
    *width = psb_xrandr_info->extend_crtc->width - 1;
    *height = psb_xrandr_info->extend_crtc->height - 1;
    *location = psb_xrandr_info->extend_crtc->location;
	
    pthread_mutex_unlock(&psb_extvideo_mutex);
    psb__information_message("Xrandr: crtc %08x coordinate: x = %d, y = %d, widht = %d, height = %d, location = %s\n",
			     psb_xrandr_info->extend_crtc->crtc_id, *x, *y, *width + 1, *height + 1, location2string(psb_xrandr_info->extend_crtc->location));
    return VA_STATUS_SUCCESS;
}

VAStatus psb_xrandr_thread_exit(Drawable draw)
{
    int ret;
    XSelectInput(dpy, draw, StructureNotifyMask);
    XClientMessageEvent xevent;
    xevent.type = ClientMessage;
    xevent.message_type = psb_exit_atom;
    xevent.window = draw;
    xevent.format = 32;
    ret = XSendEvent(dpy, draw, 0, 0, (XEvent*)&xevent);
    XFlush(dpy);
    if (!ret) {
	psb__information_message("Xrandr: send thread exit event to drawable: %08x failed\n", draw);
	return VA_STATUS_ERROR_UNKNOWN;
    }
    else {
	psb__information_message("Xrandr: send thread exit event to drawable: %08x success\n", draw);
	return VA_STATUS_SUCCESS;
    }
}

VAStatus psb_xrandr_thread_create(VADriverContextP ctx)
{
    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;
    pthread_t id;

    pthread_create(&id, NULL, (void*)psb_xrandr_thread, NULL);
    driver_data->xrandr_thread_id = id;
    return VA_STATUS_SUCCESS;
}

VAStatus psb_xrandr_deinit()
{
    pthread_mutex_lock(&psb_extvideo_mutex);
    //free crtc
    if (crtc_head)
    {
	while (crtc_head)
	{
	    crtc_tail = crtc_head->next;

	    free(crtc_head);
	
	    crtc_head = crtc_tail;
	}
	crtc_head = crtc_tail = NULL;
    }

    //free output
    if (output_head)
    {
	while (output_head)
	{
	    output_tail = output_head->next;

	    free(output_head);
	
	    output_head = output_tail;
	}
	output_head = output_tail = NULL;
    }

    if (psb_xrandr_info->hdmi_extvideo_prop)
	free(psb_xrandr_info->hdmi_extvideo_prop);

    if (psb_xrandr_info)
	free(psb_xrandr_info);

    pthread_mutex_unlock(&psb_extvideo_mutex);
    pthread_mutex_destroy(&psb_extvideo_mutex);

    return VA_STATUS_SUCCESS;
}

VAStatus psb_xrandr_init (VADriverContextP ctx)
{
    int	major, minor;
    int screen;

    dpy = (Display *)ctx->native_dpy;
    screen = DefaultScreen (dpy);
    psb_exit_atom = XInternAtom(dpy, "psb_exit_atom", 0);

    if (screen >= ScreenCount (dpy)) {
	psb__error_message("Xrandr: Invalid screen number %d (display has %d)\n",
			    screen, ScreenCount (dpy));
	return VA_STATUS_ERROR_UNKNOWN;
    }

    root = RootWindow (dpy, screen);

    if (!XRRQueryVersion (dpy, &major, &minor))
    {
	psb__error_message("Xrandr: RandR extension missing\n");
	return VA_STATUS_ERROR_UNKNOWN;
    }

    res = XRRGetScreenResources (dpy, root);
    if (!res)
	psb__error_message("Xrandr: failed to get screen resources\n");

    pthread_mutex_init(&psb_extvideo_mutex, NULL);

    psb_xrandr_refresh();

    /*while(1) { sleep(1);
	show_current();
    }*/

    return 0;
}
