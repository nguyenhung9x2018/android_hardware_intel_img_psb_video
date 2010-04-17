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


#include <va/va_backend.h>
#include "psb_surface.h"
#include "psb_output.h"
#include "psb_overlay.h"
#include "psb_xvva.h"

#ifndef ANDROID
#include <X11/extensions/dpms.h>
#endif

#include <wsbm/wsbm_manager.h>

#define LOG_TAG "psb_xvva"
#include <cutils/log.h>

#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;
#define SURFACE(id)	((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))

static int psb_CheckDrawable(VADriverContextP ctx, Drawable draw);


#ifndef ANDROID
static int GetPortId(Display *dpy, VADriverContextP ctx)
{
    int i, j, k;
    unsigned int numAdapt;
    int numImages; 
    XvImageFormatValues *formats;
    XvAdaptorInfo *info;
    psb_output_p output = GET_OUTPUT_DATA(ctx);
 
    if(Success != XvQueryAdaptors(dpy,DefaultRootWindow(dpy),&numAdapt,&info)) {
        psb__error_message("Can't find Xvideo adaptor\n");
	return -1;
    }

    for(i = 0; i < numAdapt; i++) {
	if(info[i].type & XvImageMask) {  
	    formats = XvListImageFormats(dpy, info[i].base_id, &numImages);
            for(j = 0; j < numImages; j++) {
                if(formats[j].id == FOURCC_XVVA) {
                    for(k = 0; k < info[i].num_ports; k++) {
                        if(Success == XvGrabPort(dpy, info[i].base_id + k, CurrentTime)) {
                            /*for textured adaptor 0*/
                            if (i == 0)
                                output->textured_portID = info[i].base_id + k;
                            /*for overlay adaptor 1*/
                            if (i == 1)
                                output->overlay_portID = info[i].base_id + k;
                            break;
                        }
                    }
                }
            }
	    XFree(formats);
	}
    }

    if ((output->textured_portID==0) && (output->overlay_portID==0)) {
        psb__information_message("Can't detect any usable Xv XVVA port\n");
	return -1;
    }
    

    return 0;
}
#endif

VAStatus psb_init_xvideo(VADriverContextP ctx)
{
#ifndef ANDROID
    INIT_DRIVER_DATA;
    psb_output_p output = GET_OUTPUT_DATA(ctx);
    int dummy;

    driver_data->cur_displaying_surface = VA_INVALID_SURFACE;
    driver_data->last_displaying_surface = VA_INVALID_SURFACE;
    
    output->textured_portID = output->overlay_portID = 0;
    if (GetPortId(ctx->x11_dpy, ctx)) {
	psb__error_message("Grab Xvideo port failed, fallback to software vaPutSurface.\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    if (output->textured_portID)
        psb__information_message("Textured Xvideo port_id = %d.\n", (unsigned int)output->textured_portID);
    if (output->overlay_portID)
        psb__information_message("Overlay  Xvideo port_id = %d.\n", (unsigned int)output->overlay_portID);

    /* by default, overlay Xv */
    if (output->textured_portID)
        output->output_method = PSB_PUTSURFACE_TEXTURE_XV;

    if (output->overlay_portID)
        output->output_method = PSB_PUTSURFACE_OVERLAY_XV;

    output->ignore_dpm = 1;
    if (getenv("PSB_VIDEO_DPMS_HACK")) {
        if (DPMSQueryExtension(ctx->x11_dpy, &dummy, &dummy)
            && DPMSCapable(ctx->x11_dpy)) {
            BOOL onoff;
            CARD16 state;
            
            DPMSInfo(ctx->x11_dpy, &state, &onoff);
            psb__information_message("DPMS is %s, monitor state=%s\n", onoff?"enabled":"disabled",
                                     (state==DPMSModeOn)?"on":(
                                         (state==DPMSModeOff)?"off":(
                                             (state==DPMSModeStandby)?"standby":(
                                                 (state==DPMSModeSuspend)?"suspend":"unknow"))));
            if (onoff)
                output->ignore_dpm = 0;
        }
    }
    
    if (getenv("PSB_VIDEO_TEXTURE_XV") && output->textured_portID) {
        psb__information_message("Putsurface force to use Textured Xvideo\n");
        output->output_method = PSB_PUTSURFACE_FORCE_TEXTURE_XV;
    }
    
    if (getenv("PSB_VIDEO_OVERLAY_XV") && output->overlay_portID) {
        psb__information_message("Putsurface force to use Overlay Xvideo\n");
        output->output_method = PSB_PUTSURFACE_FORCE_OVERLAY_XV;
    }
    
    return VA_STATUS_SUCCESS;
#else
    return VA_STATUS_ERROR_UNKNOWN;
#endif
}


VAStatus psb_deinit_xvideo(VADriverContextP ctx)
{
#ifndef ANDROID
    psb_output_p output = GET_OUTPUT_DATA(ctx);

    if (output->gc) {
	XFreeGC(ctx->x11_dpy, output->gc);
        output->gc = NULL;
    }

    if (output->textured_xvimage) {
        psb__information_message("Destroy XvImage for texture Xv\n");
	XFree(output->textured_xvimage);
        output->textured_xvimage = NULL;
    }
    
    if (output->overlay_xvimage) {
        psb__information_message("Destroy XvImage for overlay  Xv\n");
	XFree(output->overlay_xvimage);
        output->textured_xvimage = NULL;
    }
    
    if (output->textured_portID) {
        if ((output->using_port == USING_TEXTURE_PORT) && output->output_drawable
            && (psb_CheckDrawable(ctx, output->output_drawable) == 0)) {
            psb__information_message("Deinit: stop textured Xvideo\n");
            XvStopVideo(ctx->x11_dpy, output->textured_portID, output->output_drawable);
        }

        psb__information_message("Deinit: ungrab textured Xvideo port\n");
	XvUngrabPort(ctx->x11_dpy, output->textured_portID, CurrentTime);
	output->textured_portID = 0;
    }
    
    if (output->overlay_portID) {
        if ((output->using_port == USING_OVERLAY_PORT) && output->output_drawable
            && (psb_CheckDrawable(ctx, output->output_drawable) == 0)) {
            psb__information_message("Deinit: stop overlay Xvideo\n");
            XvStopVideo(ctx->x11_dpy, output->overlay_portID, output->output_drawable);
        }

        psb__information_message("Deinit: ungrab overlay Xvideo port\n");
	XvUngrabPort(ctx->x11_dpy, output->overlay_portID, CurrentTime);
	output->overlay_portID = 0;
    }
    output->using_port = 0;
    output->output_drawable = 0;
    
    XSync(ctx->x11_dpy, False);

    return VA_STATUS_SUCCESS;
#else
    return VA_STATUS_ERROR_UNKNOWN;
#endif
}


static void psb_surface_init(
    psb_driver_data_p driver_data,
    PsbVASurfaceRec *srf,
    int fourcc,int bpp, int w, int h,int stride, int size, unsigned int pre_add,
    struct _WsbmBufferObject *bo, int flags
                             )
{
    psb_output_p output = (psb_output_p)driver_data->video_output;

    memset(srf,0,sizeof(*srf));
  
    srf->fourcc = fourcc;
    srf->bo = bo;
    if (bo != NULL) {
        srf->bufid = wsbmKBufHandle(wsbmKBuf(bo));
        srf->pl_flags = wsbmBOPlacementHint(bo);
    }
    
    if (srf->pl_flags & DRM_PSB_FLAG_MEM_CI) 
        srf->reserved_phyaddr = driver_data->camera_phyaddr;
    if (srf->pl_flags & DRM_PSB_FLAG_MEM_RAR) 
        srf->reserved_phyaddr = driver_data->rar_phyaddr;

    srf->bytes_pp = bpp;

    srf->width = w;
    srf->pre_add = pre_add;
    if ((flags==VA_TOP_FIELD) || (flags==VA_BOTTOM_FIELD)) {
        if (output->output_method ==  PSB_PUTSURFACE_FORCE_OVERLAY_XV
            || output->output_method == PSB_PUTSURFACE_OVERLAY_XV)
        {
            srf->height = h;
            srf->stride = stride;
        }
        else
        {
            srf->height = h/2;
            srf->stride = stride*2;
        }
        if (flags == VA_BOTTOM_FIELD)
            srf->pre_add += stride;
    } else {
        srf->height = h;
        srf->stride = stride;
    }

    srf->size = size;
    
    if (flags == VA_CLEAR_DRAWABLE) {
        srf->clear_color = output->clear_color; /* color */
        return;
    }
}

#ifndef ANDROID
#if 0

#define WINDOW 1
#define PIXMAP 0

/* return 0 for no rotation, 1 for rotation occurs */
/* XRRGetScreenInfo has significant performance drop */
static int  psb__CheckCurrentRotation(VADriverContextP ctx)
{
    Rotation current_rotation;
    XRRScreenConfiguration *scrn_cfg;
    scrn_cfg = XRRGetScreenInfo(ctx->x11_dpy, DefaultRootWindow(ctx->x11_dpy));
    XRRConfigCurrentConfiguration (scrn_cfg, &current_rotation);
    XRRFreeScreenConfigInfo(scrn_cfg);
    return (current_rotation & 0x0f);
}

/* Check drawable type, 1 for window, 0 for pixmap
 * Have significant performance drop in XFCE environment
 */
static void psb__CheckDrawableType(Display *dpy, Window win, Drawable draw, int *type_ret)
{

    unsigned int child_num;
    Window root_return;
    Window parent_return;
    Window *child_return;
    int i;

    if (win == draw) {
	*type_ret = 1;
	return;
    }

    XQueryTree(dpy, win, &root_return, &parent_return, &child_return, &child_num);

    if (!child_num)
	return;

    for(i =0; i< child_num; i++)
	psb__CheckDrawableType(dpy, child_return[i], draw, type_ret);
}
#endif


static int psb_CheckDrawable(VADriverContextP ctx, Drawable draw)
{
    psb_output_p output = GET_OUTPUT_DATA(ctx);
    Atom xvDrawable = XInternAtom(ctx->x11_dpy, "XV_DRAWABLE", 0);
    int val = 0;

    output->drawable_info = 0;
    if (output->overlay_portID) {
        XvSetPortAttribute(ctx->x11_dpy, output->overlay_portID, xvDrawable, draw);
        XvGetPortAttribute(ctx->x11_dpy, output->overlay_portID, xvDrawable, &val);
    } else if (output->textured_portID) {
        XvSetPortAttribute(ctx->x11_dpy, output->textured_portID, xvDrawable, draw);
        XvGetPortAttribute(ctx->x11_dpy, output->textured_portID, xvDrawable, &val);
    }
    output->drawable_info = val;

    psb__information_message("Get xvDrawable = 0x%08x\n", val);
    
    if (output->drawable_info == XVDRAWABLE_INVALID_DRAWABLE)
        return -1;
            
    return 0;
}
#endif

static int psb__CheckPutSurfaceXvPort(
    VADriverContextP ctx,
    VASurfaceID surface,
    Drawable draw, /* X Drawable */
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
    psb_output_p output = GET_OUTPUT_DATA(ctx);
    object_surface_p obj_surface = SURFACE(surface);
    uint32_t buf_pl;
    
    /* silent klockwork */
    if (obj_surface && obj_surface->psb_surface)
        buf_pl = obj_surface->psb_surface->buf.pl_flags;
    else
        return -1;
    
    if (flags & VA_CLEAR_DRAWABLE)
        return 0;
    
#ifndef ANDROID
    if (output->overlay_portID == 0) { /* no overlay usable */
        output->output_method = PSB_PUTSURFACE_TEXTURE_XV;
        return 0;
    }
    
    if (output->output_method == PSB_PUTSURFACE_FORCE_OVERLAY_XV) {
        psb__information_message("Force Overlay Xvideo for PutSurface\n");
        return 0;
    }

    if ((output->output_method == PSB_PUTSURFACE_FORCE_TEXTURE_XV)) {
        psb__information_message("Force Textured Xvideo for PutSurface\n");
        return 0;
    }
    
    if (((buf_pl & (WSBM_PL_FLAG_TT | DRM_PSB_FLAG_MEM_RAR | DRM_PSB_FLAG_MEM_CI)) == 0) /* buf not in TT/RAR or CI */
        || (obj_surface->width > 1920)  /* overlay have isue to support >1920xXXX resolution */
        || (obj_surface->subpic_count > 0)  /* overlay can't support subpicture */
        /*    || (flags & (VA_TOP_FIELD|VA_BOTTOM_FIELD))*/
        ) {
        output->output_method = PSB_PUTSURFACE_TEXTURE_XV;
        return 0;
    }
#else
    output->overlay_portID = 1; // force overlay use.

    if (((buf_pl & (WSBM_PL_FLAG_TT | DRM_PSB_FLAG_MEM_RAR | DRM_PSB_FLAG_MEM_CI)) == 0) /* buf not in TT/RAR or CI */
        || (obj_surface->width > 1920)  /* overlay can't support subpicture */
        || (obj_surface->subpic_count > 0)  /* overlay can't support subpicture */
        ) {
        output->output_method = PSB_PUTSURFACE_TEXTURE_XV;
        LOGE("ERROR: %s() Overlay can not support subpicture or width>1920",__func__);
        return -1;
    }
#endif
        

    /* Here should be overlay XV by defaut after overlay is stable */
    output->output_method = PSB_PUTSURFACE_OVERLAY_XV;
    /* output->output_method = PSB_PUTSURFACE_TEXTURE_XV; */

#ifndef ANDROID
    /*
     *Query Overlay Adaptor by XvDrawable Attribute to know current
     * Xrandr information:rotation/downscaling
     * also set target drawable(window vs pixmap) into XvDrawable
     * to levage Xserver to determiate it is Pixmap or Window
     */
    /*
     *ROTATE_90: 0x2, ROTATE_180: 0x4, ROTATE_270:0x8
     *Overlay adopator can support video rotation, 
     *but its performance is lower than texture video path.
     *When video protection and rotation are required (use RAR buffer),
     *only overlay adaptor will be used.
     *other attribute like down scaling and pixmap, use texture adaptor
     */     
    if (output->drawable_info
        & (XVDRAWABLE_ROTATE_180 | XVDRAWABLE_ROTATE_90 | XVDRAWABLE_ROTATE_270)) {
        if (buf_pl & DRM_PSB_FLAG_MEM_RAR)
            output->output_method = PSB_PUTSURFACE_OVERLAY_XV;
        else
            output->output_method = PSB_PUTSURFACE_TEXTURE_XV;
    }	

    if (output->drawable_info & (XVDRAWABLE_PIXMAP | XVDRAWABLE_REDIRECT_WINDOW))
        output->output_method = PSB_PUTSURFACE_TEXTURE_XV;

    if (srcw >= destw * 8 || srch >= desth * 8)
	output->output_method = PSB_PUTSURFACE_TEXTURE_XV;
#endif

    return 0;
}

#ifndef ANDROID
static int psb__CheckGCXvImage(
    VADriverContextP ctx,
    VASurfaceID surface,
    Drawable draw,
    XvImage **xvImage,
    XvPortID *port_id,
    unsigned int flags /* de-interlacing flags */
                               )
{
    INIT_DRIVER_DATA;
    psb_output_p output = GET_OUTPUT_DATA(ctx);
    object_surface_p obj_surface = SURFACE(surface); /* surface already checked */

    if (output->output_drawable != draw) {
        if (output->gc)
            XFreeGC(ctx->x11_dpy, output->gc);
	output->gc = XCreateGC (ctx->x11_dpy, draw, 0, NULL);
        output->output_drawable = draw;
    }

    if (flags & VA_CLEAR_DRAWABLE) {
        if (output->textured_portID && (output->using_port == USING_TEXTURE_PORT)) {
            psb__information_message("Clear drawable, and stop textured Xvideo\n");
            XvStopVideo(ctx->x11_dpy, output->textured_portID, draw);
        }
        
        if (output->overlay_portID && (output->using_port == USING_OVERLAY_PORT)) {
            psb__information_message("Clear drawable, and stop overlay Xvideo\n");
            XvStopVideo(ctx->x11_dpy, output->overlay_portID, draw);
        }
        
        output->using_port = 0;

        XSetForeground(ctx->x11_dpy, output->gc, output->clear_color);
        
        return 0;
    }
    
    if ((output->output_method == PSB_PUTSURFACE_FORCE_OVERLAY_XV)||
        (output->output_method == PSB_PUTSURFACE_OVERLAY_XV)) {
        /* use OVERLAY XVideo */
        if (obj_surface &&
            ((output->output_width != obj_surface->width) ||
             (output->output_height != obj_surface->height) ||
             (!output->overlay_xvimage))) {
        
            if (output->overlay_xvimage)
                XFree(output->overlay_xvimage);

            psb__information_message("Create new XvImage for overlay\n");
            output->overlay_xvimage = XvCreateImage(ctx->x11_dpy, output->overlay_portID,
                                                    FOURCC_XVVA, 0,
                                                    obj_surface->width, obj_surface->height);

            output->overlay_xvimage->data = (char *)&output->imgdata_vasrf;
            output->output_width = obj_surface->width;
            output->output_height = obj_surface->height;
        }
        *xvImage = output->overlay_xvimage;
        *port_id = output->overlay_portID;

        if ((output->textured_portID) && (output->using_port == USING_TEXTURE_PORT)) { /* stop texture port */
            psb__information_message("Using overlay xvideo, stop textured xvideo\n");
            XvStopVideo(ctx->x11_dpy, output->textured_portID, draw);
            XSync(ctx->x11_dpy, False);
        }
        output->using_port = USING_OVERLAY_PORT;
        
        psb__information_message("Using Overlay Xvideo for PutSurface\n");

        return 0;
    }
    
    if ((output->output_method == PSB_PUTSURFACE_FORCE_TEXTURE_XV)||
        (output->output_method == PSB_PUTSURFACE_TEXTURE_XV)) {
        /* use Textured XVideo */
        if (obj_surface && 
            ((output->output_width != obj_surface->width) ||
             (output->output_height != obj_surface->height ||
              (!output->textured_xvimage)))) {
            if (output->textured_xvimage)
                XFree(output->textured_xvimage);

            psb__information_message("Create new XvImage for overlay\n");
            output->textured_xvimage = XvCreateImage(ctx->x11_dpy, output->textured_portID, FOURCC_XVVA, 0,
                                                     obj_surface->width, obj_surface->height);
            output->textured_xvimage->data = (char *)&output->imgdata_vasrf;
            output->output_width = obj_surface->width;
            output->output_height = obj_surface->height;
        
        }
        
        *xvImage = output->textured_xvimage;
        *port_id = output->textured_portID;

        if ((output->overlay_portID) && (output->using_port == USING_OVERLAY_PORT)) { /* stop overlay port */
            psb__information_message("Using textured xvideo, stop Overlay xvideo\n");
            XvStopVideo(ctx->x11_dpy, output->overlay_portID, draw);
            XSync(ctx->x11_dpy, False);

            output->using_port = USING_TEXTURE_PORT;
        }
        
        psb__information_message("Using Texture Xvideo for PutSurface\n");
        
        return 0;
    }

    return 0;
}

static int psb_force_dpms_on(VADriverContextP ctx)
{
    BOOL onoff;
    CARD16 state;

    DPMSInfo(ctx->x11_dpy, &state, &onoff);
    psb__information_message("DPMS is %s, monitor state=%s\n", onoff?"enabled":"disabled",
                             (state==DPMSModeOn)?"on":(
                                 (state==DPMSModeOff)?"off":(
                                     (state==DPMSModeStandby)?"standby":(
                                         (state==DPMSModeSuspend)?"suspend":"unknow"))));
    if (onoff && (state != DPMSModeOn)) {
        psb__information_message("DPMS is enabled, and monitor isn't DPMSModeOn, force it on\n");
        DPMSForceLevel(ctx->x11_dpy, DPMSModeOn);
    }
    
    return 0;
}

static int unsigned long GetTickCount()
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL))
        return 0;
    return tv.tv_usec/1000+tv.tv_sec*1000;
}
#endif

VAStatus psb_putsurface_xvideo(
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
#ifndef ANDROID
    INIT_DRIVER_DATA;
    psb_output_p output = GET_OUTPUT_DATA(ctx);
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    PsbVASurfaceRec *subpic_surface;
    PsbXvVAPutSurfacePtr vaPtr;
    XvPortID 	portID = 0;
    XvImage *xvImage = NULL;
    object_surface_p obj_surface = SURFACE(surface);
    psb_surface_p psb_surface;
    int i=0, j;

    if (obj_surface) /* silent klockwork, we already check it */
        psb_surface = obj_surface->psb_surface;
    else
        return VA_STATUS_ERROR_UNKNOWN;
    
    if (psb_CheckDrawable(ctx, draw) != 0) {
        psb__error_message("vaPutSurface: invalidate drawable\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    pthread_mutex_lock(&output->output_mutex);    
    psb__CheckPutSurfaceXvPort(ctx, surface, draw,
                               srcx, srcy, srcw, srch,
                               destx, desty, destw, desth,
                               cliprects, number_cliprects, flags);
    psb__CheckGCXvImage(ctx, surface, draw, &xvImage, &portID, flags);

    if (flags & VA_CLEAR_DRAWABLE) {
        psb__information_message("Clean draw with color 0x%08x\n",output->clear_color);
        
        XFillRectangle(ctx->x11_dpy, draw, output->gc, destx, desty, destw, desth);
        XSync(ctx->x11_dpy, False);

        XFreeGC(ctx->x11_dpy, output->gc);
        output->gc = NULL;
        output->output_drawable = 0;
        
        XSync(ctx->x11_dpy, False);

        driver_data->cur_displaying_surface = VA_INVALID_SURFACE;
        driver_data->last_displaying_surface = VA_INVALID_SURFACE;
        obj_surface->display_timestamp = 0;

        pthread_mutex_unlock(&output->output_mutex);
        
        return vaStatus;
    }
    
    vaPtr = (PsbXvVAPutSurfacePtr)xvImage->data;
    vaPtr->flags = flags;
    vaPtr->num_subpicture = obj_surface->subpic_count;
    vaPtr->num_clipbox = number_cliprects;
    for (j = 0; j < number_cliprects; j++)
    {
        vaPtr->clipbox[j].x = cliprects[j].x;
        vaPtr->clipbox[j].y = cliprects[j].y;
        vaPtr->clipbox[j].width = cliprects[j].width;
        vaPtr->clipbox[j].height = cliprects[j].height;
    }

    psb_surface_init(driver_data,&vaPtr->src_srf,VA_FOURCC_NV12,2,
                     obj_surface->width,obj_surface->height,
                     psb_surface->stride, psb_surface->size,
                     psb_surface->buf.buffer_ofs, /* for surface created from RAR/camera device memory
                                                   * all surfaces share one BO but with different offset
                                                   * pass the offset as the "pre_add"
                                                   */
                     psb_surface->buf.drm_buf, flags);

    if ((output->output_method == PSB_PUTSURFACE_OVERLAY_XV) 
        && (output->drawable_info & (XVDRAWABLE_ROTATE_180 | XVDRAWABLE_ROTATE_90 | XVDRAWABLE_ROTATE_270))) {
        object_surface_p obj_rotate_surface;
        unsigned int rotate_width, rotate_height;
        if (output->drawable_info & (XVDRAWABLE_ROTATE_90 | XVDRAWABLE_ROTATE_270)) {
            rotate_width = obj_surface->height;
            rotate_height = obj_surface->width;
        } else {
            rotate_width = obj_surface->width;
            rotate_height = obj_surface->height;
        }		
        if (output->rotate_surface) {
            obj_rotate_surface = SURFACE(output->rotate_surfaceID);
            if (obj_rotate_surface && 
                ((obj_rotate_surface->width != obj_surface->width)
                 || (obj_rotate_surface->height != obj_surface->height))) {
                psb_surface_destroy(output->rotate_surface);
                free(output->rotate_surface);
                object_heap_free(&driver_data->surface_heap, (object_base_p)obj_rotate_surface);
                output->rotate_surface = NULL;
            }
        }
        if (output->rotate_surface == NULL) {
            unsigned int protected;

            output->rotate_surfaceID = object_heap_allocate(&driver_data->surface_heap);
            obj_rotate_surface = SURFACE(output->rotate_surfaceID);
            if (NULL == obj_rotate_surface)
            {
                vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
                DEBUG_FAILURE;
                
                pthread_mutex_unlock(&output->output_mutex);
                return VA_STATUS_ERROR_ALLOCATION_FAILED;
            }
	        
            obj_rotate_surface->surface_id = output->rotate_surfaceID;
            obj_rotate_surface->context_id = -1;
            obj_rotate_surface->width = obj_surface->width;
            obj_rotate_surface->height = obj_surface->height;
            obj_rotate_surface->subpictures = NULL;
            obj_rotate_surface->subpic_count = 0; 
            obj_rotate_surface->derived_imgcnt = 0;
            output->rotate_surface = (psb_surface_p) malloc(sizeof(struct psb_surface_s));
            if (NULL == output->rotate_surface)
            {
                object_heap_free( &driver_data->surface_heap, (object_base_p) obj_rotate_surface);
                obj_rotate_surface->surface_id = VA_INVALID_SURFACE;

                vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;

                DEBUG_FAILURE;

                pthread_mutex_unlock(&output->output_mutex);

                return VA_STATUS_ERROR_ALLOCATION_FAILED;
            }
            memset(output->rotate_surface, 0, sizeof(struct psb_surface_s));
            protected = vaPtr->src_srf.pl_flags & DRM_PSB_FLAG_MEM_RAR;
            vaStatus = psb_surface_create( driver_data, rotate_width, rotate_height,
                                           VA_FOURCC_RGBA, protected, output->rotate_surface
                                           );
            obj_rotate_surface->psb_surface = output->rotate_surface;
        }

        psb_surface_init(driver_data, &vaPtr->dst_srf, VA_FOURCC_RGBA, 4,
                         rotate_width, rotate_height,
                         output->rotate_surface->stride, output->rotate_surface->size,
                         output->rotate_surface->buf.buffer_ofs, /* for surface created from RAR/camera device memory
                                                                  * all surfaces share one BO but with different offset
                                                                  * pass the offset as the "pre_add"
                                                                  */
                         output->rotate_surface->buf.drm_buf, 0);	

    }
   
    subpic_surface = obj_surface->subpictures;
    while (subpic_surface) {
        PsbVASurfaceRec *tmp = &vaPtr->subpic_srf[i++];
        
        memcpy(tmp, subpic_surface, sizeof(*tmp));

        /* reload palette for paletted subpicture
         * palete_ptr point to image palette
         */
        if (subpic_surface->palette_ptr)
            memcpy(&tmp->palette[0], subpic_surface->palette_ptr, 16 * sizeof(PsbAYUVSample8));
        
	subpic_surface = subpic_surface->next;
    }

    if (output->ignore_dpm == 0)
        psb_force_dpms_on(ctx);
    
    XvPutImage(ctx->x11_dpy, portID, draw, output->gc, xvImage,
               srcx, srcy, srcw, srch, destx, desty, destw, desth);

    XSync(ctx->x11_dpy, False);

    if (portID == output->overlay_portID) {
        if (driver_data->cur_displaying_surface != VA_INVALID_SURFACE) 
            driver_data->last_displaying_surface = driver_data->cur_displaying_surface;
        obj_surface->display_timestamp = GetTickCount();
        driver_data->cur_displaying_surface = surface;
    } else {
        driver_data->cur_displaying_surface = VA_INVALID_SURFACE;
        driver_data->last_displaying_surface = VA_INVALID_SURFACE;
        obj_surface->display_timestamp = 0;
    }
    
    pthread_mutex_unlock(&output->output_mutex);    
            
    return vaStatus;
#else
    return VA_STATUS_ERROR_UNKNOWN;
#endif
}

#ifdef ANDROID
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
    PsbVASurfaceRec *subpic_surface;
    PsbXvVAPutSurfacePtr vaPtr;
    XvPortID    portID = 0;
    object_surface_p obj_surface = SURFACE(surface);
    psb_surface_p psb_surface = obj_surface->psb_surface;
    int i=0, j;
    int id;

    LOGV("%s(): Enter", __func__);
    pthread_mutex_lock(&output->output_mutex);
    psb__CheckPutSurfaceXvPort(ctx, surface, draw,
                               srcx, srcy, srcw, srch,
                               destx, desty, destw, desth,
                               cliprects, number_cliprects, flags);

    if (output->output_drawable != draw)
        output->output_drawable = draw;

    if (flags & VA_CLEAR_DRAWABLE) {
        if (output->overlay_portID)
            I830StopVideo(ctx);
        return 0;
    }

    if ((output->output_method == PSB_PUTSURFACE_FORCE_OVERLAY_XV)||
        (output->output_method == PSB_PUTSURFACE_OVERLAY_XV)) {
        /* use OVERLAY XVideo */
        if ((output->output_width != obj_surface->width) ||
            (output->output_height != obj_surface->height)) {

            output->output_width = obj_surface->width;
            output->output_height = obj_surface->height;
        }
        portID = output->overlay_portID;
        output->using_port = USING_OVERLAY_PORT;

        LOGV("%s(): Using Overlay Xvideo for PutSurface",__func__);
    }

    if (flags & VA_CLEAR_DRAWABLE) {
        psb__information_message("Clean draw with color 0x%08x\n",output->clear_color);

        driver_data->cur_displaying_surface = VA_INVALID_SURFACE;
        driver_data->last_displaying_surface = VA_INVALID_SURFACE;
        obj_surface->display_timestamp = 0;

        return vaStatus;
    }

    vaPtr = (PsbXvVAPutSurfacePtr)&output->imgdata_vasrf;
    vaPtr->flags = flags;
    vaPtr->num_subpicture = obj_surface->subpic_count;
    vaPtr->num_clipbox = number_cliprects;
    for (j = 0; j < number_cliprects; j++)
    {
        vaPtr->clipbox[j].x = cliprects[j].x;
        vaPtr->clipbox[j].y = cliprects[j].y;
        vaPtr->clipbox[j].width = cliprects[j].width;
        vaPtr->clipbox[j].height = cliprects[j].height;
    }

    psb_surface_init(driver_data,&vaPtr->src_srf,VA_FOURCC_NV12,2,
                     obj_surface->width,obj_surface->height,
                     psb_surface->stride, psb_surface->size,
                     psb_surface->buf.buffer_ofs, /* for surface created from RAR/camera device memory
                                                   * all surfaces share one BO but with different offset
                                                   * pass the offset as the "pre_add"
                                                   */
                     psb_surface->buf.drm_buf, flags);

    subpic_surface = obj_surface->subpictures;
    while (subpic_surface) {
        PsbVASurfaceRec *tmp = &vaPtr->subpic_srf[i++];

        memcpy(tmp, subpic_surface, sizeof(*tmp));

        /* reload palette for paletted subpicture
         * palete_ptr point to image palette
         */
        if (subpic_surface->palette_ptr)
            memcpy(&tmp->palette[0], subpic_surface->palette_ptr, 16 * sizeof(PsbAYUVSample8));

	subpic_surface = subpic_surface->next;
    }

    /* FIXME: Set XVVA as default */
    id = FOURCC_XVVA;

#if USE_FIT_SCR_SIZE
    {
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
    }
#endif

    /* psb_force_dpms_on(ctx); */
    I830PutImage(ctx, surface, srcx, srcy, srcw, srch, destx, desty, destw, desth, id);

    if (portID == output->overlay_portID) {
        if (driver_data->cur_displaying_surface != VA_INVALID_SURFACE)
            driver_data->last_displaying_surface = driver_data->cur_displaying_surface;
        obj_surface->display_timestamp = 0;
        driver_data->cur_displaying_surface = surface;
    } else {
        driver_data->cur_displaying_surface = VA_INVALID_SURFACE;
        driver_data->last_displaying_surface = VA_INVALID_SURFACE;
        obj_surface->display_timestamp = 0;
    }

    pthread_mutex_unlock(&output->output_mutex);

    return vaStatus;
}
#endif
