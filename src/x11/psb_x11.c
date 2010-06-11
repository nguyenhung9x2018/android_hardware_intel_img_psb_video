
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

#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <va/va_backend.h>
#include "psb_output.h"
#include "psb_surface.h"
#include "psb_buffer.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "psb_xvva.h"
#include <wsbm/wsbm_manager.h>

#define INIT_DRIVER_DATA	psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;

#define SURFACE(id)	((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &driver_data->buffer_heap, id ))
#define IMAGE(id)  ((object_image_p) object_heap_lookup( &driver_data->image_heap, id ))
#define SUBPIC(id)  ((object_subpic_p) object_heap_lookup( &driver_data->subpic_heap, id ))
#define CONTEXT(id) ((object_context_p) object_heap_lookup( &driver_data->context_heap, id ))

static  struct timeval tftarget = {0};

static uint32_t mask2shift(uint32_t mask)
{
    uint32_t shift = 0;
    while((mask & 0x1) == 0)
    {
        mask = mask >> 1;
        shift++;
    }
    return shift;
}

static void psb_doframerate(int fps)
{
    struct timeval tfdiff;

    tftarget.tv_usec += 1000000 / fps;
    /* this is where we should be */
    if (tftarget.tv_usec >= 1000000) {
        tftarget.tv_usec -= 1000000;
        tftarget.tv_sec++;
    }

    /* this is where we are */
    gettimeofday(&tfdiff,(struct timezone *)NULL);

    tfdiff.tv_usec = tftarget.tv_usec - tfdiff.tv_usec;
    tfdiff.tv_sec  = tftarget.tv_sec  - tfdiff.tv_sec;
    if (tfdiff.tv_usec < 0) {
        tfdiff.tv_usec += 1000000;
        tfdiff.tv_sec--;
    }

    /* See if we are already lagging behind */
    if (tfdiff.tv_sec < 0 || (tfdiff.tv_sec == 0 && tfdiff.tv_usec <= 0))
        return;

    /* Spin for awhile */
    select(0,NULL,NULL,NULL,&tfdiff);
}

static VAStatus psb_putsurface_x11(
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
    unsigned int flags /* de-interlacing flags */
)
{
    INIT_DRIVER_DATA;
    GC gc;
    XImage *ximg = NULL;
    Visual *visual;
    unsigned short width, height;
    int depth;
    int x = 0,y = 0;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    void *surface_data = NULL;
    int ret;
    
    uint32_t rmask = 0;
    uint32_t gmask = 0;
    uint32_t bmask = 0;
    
    uint32_t rshift = 0;
    uint32_t gshift = 0;
    uint32_t bshift = 0;

    void yuv2pixel(uint32_t *pixel, int y, int u, int v)
    {
        int r, g, b;
        /* Warning, magic values ahead */
        r = y + ((351 * (v-128)) >> 8);
        g = y - (((179 * (v-128)) + (86 * (u-128))) >> 8);
        b = y + ((444 * (u-128)) >> 8);
	
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;
			
        *pixel = ((r << rshift) & rmask) | ((g << gshift) & gmask) | ((b << bshift) & bmask);
    }

    if (srcw <= destw)
        width = srcw;
    else
        width = destw;

    if (srch <= desth)
        height = srch;
    else
        height = desth;

    object_surface_p obj_surface = SURFACE(surface);
    if (NULL == obj_surface)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    psb_surface_p psb_surface = obj_surface->psb_surface;

    psb__information_message("PutSurface: src	  w x h = %d x %d\n", srcw, srch);
    psb__information_message("PutSurface: dest 	  w x h = %d x %d\n", destw, desth);
    psb__information_message("PutSurface: clipped w x h = %d x %d\n", width, height);
        
    visual = DefaultVisual((Display *)ctx->native_dpy, ctx->x11_screen);
    gc = XCreateGC((Display *)ctx->native_dpy, draw, 0, NULL);
    depth = DefaultDepth((Display *)ctx->native_dpy, ctx->x11_screen);

    if (TrueColor != visual->class)
    {
        psb__error_message("PutSurface: Default visual of X display must be TrueColor.\n");
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        goto out;
    }
    
    ret = psb_buffer_map(&psb_surface->buf, &surface_data);
    if (ret)
    {
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        goto out;
    }
    
    rmask = visual->red_mask;
    gmask = visual->green_mask;
    bmask = visual->blue_mask;

    rshift = mask2shift(rmask);
    gshift = mask2shift(gmask);
    bshift = mask2shift(bmask);
    
    psb__information_message("PutSurface: Pixel masks: R = %08x G = %08x B = %08x\n", rmask, gmask, bmask); 
    psb__information_message("PutSurface: Pixel shifts: R = %d G = %d B = %d\n", rshift, gshift, bshift); 
    
    ximg = XCreateImage((Display *)ctx->native_dpy, visual, depth, ZPixmap, 0, NULL, width, height, 32, 0 );

    if (ximg->byte_order == MSBFirst)
        psb__information_message("PutSurface: XImage pixels has MSBFirst, %d bits / pixel\n", ximg->bits_per_pixel);
    else
        psb__information_message("PutSurface: XImage pixels has LSBFirst, %d bits / pixel\n", ximg->bits_per_pixel);

    if (ximg->bits_per_pixel != 32)
    {
        psb__error_message("PutSurface: Display uses %d bits/pixel which is not supported\n");
        vaStatus = VA_STATUS_ERROR_UNKNOWN;
        goto out;
    }
    
    ximg->data = (char *) malloc(ximg->bytes_per_line * height);
    if (NULL == ximg->data)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto out;
    }

    uint8_t *src_y = surface_data + psb_surface->stride * srcy;
    uint8_t *src_uv = surface_data + psb_surface->stride * (obj_surface->height + srcy / 2);

    for(y = srcy; y < (srcy+height); y += 2)
    {
        uint32_t *dest_even = (uint32_t *) (ximg->data + y * ximg->bytes_per_line);
        uint32_t *dest_odd = (uint32_t *) (ximg->data + (y + 1) * ximg->bytes_per_line);
        for(x = srcx; x < (srcx+width); x += 2)
        {
            /* Y1 Y2 */
            /* Y3 Y4 */
            int y1 = *(src_y + x);
            int y2 = *(src_y + x + 1);
            int y3 = *(src_y + x + psb_surface->stride);
            int y4 = *(src_y + x + psb_surface->stride + 1);

            /* U V */
            int u = *(src_uv + x);
            int v = *(src_uv + x +1 );
			
            yuv2pixel(dest_even++, y1, u, v);
            yuv2pixel(dest_even++, y2, u, v);

            yuv2pixel(dest_odd++, y3, u, v);
            yuv2pixel(dest_odd++, y4, u, v);
        }
        src_y += psb_surface->stride * 2;
        src_uv += psb_surface->stride;
    }
    
    XPutImage((Display *)ctx->native_dpy, draw, gc, ximg, 0, 0, destx, desty, width, height);
    XFlush((Display *)ctx->native_dpy);
                  
  out:
    if (NULL != ximg)
    {
        XDestroyImage(ximg);
    }
    if (NULL != surface_data)
    {
        psb_buffer_unmap(&psb_surface->buf);
    }
    XFreeGC((Display *)ctx->native_dpy, gc);
    
    return vaStatus;
}

typedef struct {
    int                 i32Left;
    int                 i32Top;
    int                 i32Right;
    int                 i32Bottom;
    unsigned int        ui32Width;
    unsigned int        ui32Height;
} psb_x11_win_t;

typedef struct x11_rect_list {
    psb_x11_win_t     rect;
    struct x11_rect_list * next;
} psb_x11_clip_list_t;

static int 
psb_x11_getWindowCoordinate(Display * display,
                            Window x11_window_id,
                            psb_x11_win_t * psX11Window,
                            int * pbIsVisible)
{
    Window DummyWindow;
    Status status;
    XWindowAttributes sXWinAttrib;
    
    if ((status = XGetWindowAttributes(display,
				       x11_window_id,
				       &sXWinAttrib)) == 0)
    {
        psb__error_message("%s: Failed to get X11 window coordinates - error %lu\n", __func__, (unsigned long)status);
        return -1;
    }

    psX11Window->i32Left = sXWinAttrib.x;
    psX11Window->i32Top = sXWinAttrib.y;
    psX11Window->ui32Width = sXWinAttrib.width;
    psX11Window->ui32Height = sXWinAttrib.height;
    *pbIsVisible = (sXWinAttrib.map_state == IsViewable);
 
    if (!*pbIsVisible)
    {
	return 0;
    }

    if (XTranslateCoordinates(display,
			      x11_window_id,
			      DefaultRootWindow(display),
			      0,
			      0,
			      &psX11Window->i32Left,
			      &psX11Window->i32Top,
			      &DummyWindow) == 0)
    {
        psb__error_message("%s: Failed to tranlate X coordinates - error %lu\n", __func__, (unsigned long)status);
        return -1;
    }

    psX11Window->i32Right  = psX11Window->i32Left + psX11Window->ui32Width - 1;
    psX11Window->i32Bottom = psX11Window->i32Top + psX11Window->ui32Height - 1;	

    return 0; 
}
static psb_x11_clip_list_t *
psb_x11_createClipBoxNode(psb_x11_win_t *       pRect,
                          psb_x11_clip_list_t * pClipNext)
{
    psb_x11_clip_list_t * pCurrent = NULL;
    
    pCurrent = (psb_x11_clip_list_t *)malloc(sizeof(psb_x11_clip_list_t));
    if (pCurrent)
    {
        pCurrent->rect = *pRect;
        pCurrent->next = pClipNext;

        return pCurrent;
    }
    else
    { 
        return pClipNext;
    }
}

static void
psb_x11_freeWindowClipBoxList(psb_x11_clip_list_t * pHead)
{
    psb_x11_clip_list_t * pNext = NULL;

    while (pHead)
    {
        pNext = pHead->next;
        free(pHead);
        pHead = pNext;
    }
}

#define IS_BETWEEN_RANGE(a,b,c) ((a<=b)&&(b<=c))

static psb_x11_clip_list_t *
psb_x11_substractRects(Display *             display,
                       psb_x11_clip_list_t * psRegion,
                       psb_x11_win_t *       psRect)
{
    psb_x11_clip_list_t * psCur,* psPrev,* psFirst,* psNext;
    psb_x11_win_t sCreateRect;
    int display_width  = (int)(DisplayWidth(display, DefaultScreen(display))) - 1;
    int display_height = (int)(DisplayHeight(display, DefaultScreen(display))) - 1;

    psFirst = psb_x11_createClipBoxNode(psRect,NULL);
  
    if(psFirst->rect.i32Left < 0)
    {
        psFirst->rect.i32Left = 0;
    }
    else if(psFirst->rect.i32Left > display_width)
    {
        psFirst->rect.i32Left = display_width;
    }
    	
    if(psFirst->rect.i32Right < 0)
    {
        psFirst->rect.i32Right = 0;
    }
    else if(psFirst->rect.i32Right > display_width)
    {
        psFirst->rect.i32Right = display_width;
    }

    if(psFirst->rect.i32Top < 0)
    {
        psFirst->rect.i32Top = 0;
    }
    else if(psFirst->rect.i32Top> display_height)
    {
        psFirst->rect.i32Top = display_height;
    }
    	
    if(psFirst->rect.i32Bottom < 0)
    {
        psFirst->rect.i32Bottom = 0;
    }
    else if(psFirst->rect.i32Bottom > display_height)
    {
        psFirst->rect.i32Bottom = display_height;
    }
    
    while(psRegion)
    {
        psCur  = psFirst;
        psPrev = NULL;
        
        while(psCur)
        {
            psNext = psCur->next;
            
            if((psRegion->rect.i32Left>psCur->rect.i32Left)&&
            	(psRegion->rect.i32Left<=psCur->rect.i32Right))
            {
                sCreateRect.i32Right=psRegion->rect.i32Left-1;
           
                sCreateRect.i32Left=psCur->rect.i32Left;
                sCreateRect.i32Top=psCur->rect.i32Top;
                sCreateRect.i32Bottom=psCur->rect.i32Bottom;
                
                psFirst = psb_x11_createClipBoxNode(&sCreateRect, psFirst);
                
                if(!psPrev)
                {
                     psPrev = psFirst;
                }
                
                psCur->rect.i32Left = psRegion->rect.i32Left;
            }
            
            if((psRegion->rect.i32Right >= psCur->rect.i32Left)&&
            	(psRegion->rect.i32Right < psCur->rect.i32Right))
            
            {
                sCreateRect.i32Left = psRegion->rect.i32Right+1;
            
                sCreateRect.i32Right = psCur->rect.i32Right;
                sCreateRect.i32Top = psCur->rect.i32Top;
                sCreateRect.i32Bottom = psCur->rect.i32Bottom;
                
                psFirst = psb_x11_createClipBoxNode(&sCreateRect, psFirst);
                
                if(!psPrev)
                {
                    psPrev = psFirst;
                }
                
                psCur->rect.i32Right = psRegion->rect.i32Right;
            }
            
            if((psRegion->rect.i32Top > psCur->rect.i32Top)&&
            	(psRegion->rect.i32Top <= psCur->rect.i32Bottom))
            {
                sCreateRect.i32Bottom = psRegion->rect.i32Top - 1;
                
                sCreateRect.i32Left   = psCur->rect.i32Left;
                sCreateRect.i32Right  = psCur->rect.i32Right;
                sCreateRect.i32Top    = psCur->rect.i32Top;
                
                psFirst = psb_x11_createClipBoxNode(&sCreateRect, psFirst);
                
                if(!psPrev)
                {
                    psPrev = psFirst;
                }
                
                psCur->rect.i32Top = psRegion->rect.i32Top;
            }
            
            if((psRegion->rect.i32Bottom >= psCur->rect.i32Top)&&
               (psRegion->rect.i32Bottom <  psCur->rect.i32Bottom))
            {
                sCreateRect.i32Top    = psRegion->rect.i32Bottom + 1;
                sCreateRect.i32Left   = psCur->rect.i32Left;
                sCreateRect.i32Right  = psCur->rect.i32Right;
                sCreateRect.i32Bottom = psCur->rect.i32Bottom;
                
                psFirst = psb_x11_createClipBoxNode(&sCreateRect, psFirst);
                
                if(!psPrev)
                {
                    psPrev = psFirst;
                }
                
                psCur->rect.i32Bottom = psRegion->rect.i32Bottom;
            }
        
            if((IS_BETWEEN_RANGE(psRegion->rect.i32Left, psCur->rect.i32Left,   psRegion->rect.i32Right)) &&
               (IS_BETWEEN_RANGE(psRegion->rect.i32Left, psCur->rect.i32Right,  psRegion->rect.i32Right)) &&
               (IS_BETWEEN_RANGE(psRegion->rect.i32Top,  psCur->rect.i32Top,    psRegion->rect.i32Bottom)) &&
               (IS_BETWEEN_RANGE(psRegion->rect.i32Top,  psCur->rect.i32Bottom, psRegion->rect.i32Bottom)))
            {
                if(psPrev)
                {
                    psPrev->next = psCur->next;
                    free(psCur);
                    psCur = psPrev;
                }
                else
                {
                    free(psCur);
                    psCur=NULL;
                    psFirst = psNext;
                }
            }
            psPrev = psCur;
            psCur  = psNext;
        }//while(psCur)
        psRegion = psRegion->next;
    }//while(psRegion)

    return psFirst;
}
	
static int 
psb_x11_createWindowClipBoxList(Display *              display,
  	                        Window                 x11_window_id,
  	                        psb_x11_clip_list_t ** ppWindowClipBoxList,
  	                        unsigned int *         pui32NumClipBoxList)
{
    Window CurrentWindow = x11_window_id; 
    Window RootWindow, ParentWindow, ChildWindow;
    Window * pChildWindow;
    Status XResult;
    int i32NumChildren, i;
    int bIsVisible;
    unsigned int ui32NumRects = 0;
    psb_x11_clip_list_t *psRegions = NULL;
    psb_x11_win_t sRect;
    
    if (!display || (!ppWindowClipBoxList)||(!pui32NumClipBoxList))
    {
        return -1;
    }  

    XResult = XQueryTree(display,
			 CurrentWindow,
			 &RootWindow,
			 &ParentWindow,
			 &pChildWindow,
		         &i32NumChildren);
    if(XResult == 0)
    {
        return -2;
    } 

    if(i32NumChildren)
    {
        for(i = 0; i < i32NumChildren; i++)
        {
            
            psb_x11_getWindowCoordinate(display, x11_window_id, &sRect, &bIsVisible);			
            if (bIsVisible)
            {
                psRegions = psb_x11_createClipBoxNode(&sRect,psRegions);
                ui32NumRects++;
            }
        }
        XFree(pChildWindow);
        i32NumChildren = 0;
    }

    while(CurrentWindow != RootWindow)
    {
        ChildWindow   = CurrentWindow;
        CurrentWindow = ParentWindow;

        XResult = XQueryTree(display,
                             CurrentWindow,
                             &RootWindow,
                             &ParentWindow,
                             &pChildWindow,
                             &i32NumChildren);
        if(XResult == 0)
        {
            if(i32NumChildren)
            {
                XFree(pChildWindow);
            }
            psb_x11_freeWindowClipBoxList(psRegions);
            return -3;
        }
   
        if(i32NumChildren)
        {
            unsigned int iStartWindow = 0;

            for(i = 0; i < i32NumChildren; i++)
            {
                if(pChildWindow[i] == ChildWindow)
                {
                    iStartWindow = i;
                    break;
                }
            }

            if(i == i32NumChildren)
            {
                XFree(pChildWindow);
                psb_x11_freeWindowClipBoxList(psRegions);
                return -4;
            }

            for(i = iStartWindow + 1; i < i32NumChildren; i++)
            {
                psb_x11_getWindowCoordinate(display, pChildWindow[i], &sRect, &bIsVisible);
                if (bIsVisible)
                {
                    psRegions = psb_x11_createClipBoxNode(&sRect, psRegions);
                    ui32NumRects++;
                }
            }

            XFree(pChildWindow);
        }
    }

    ui32NumRects = 0;

    if(psRegions)
    {
        psb_x11_getWindowCoordinate(display, x11_window_id, &sRect, &bIsVisible);			
        *ppWindowClipBoxList = psb_x11_substractRects(display, psRegions, &sRect);
        psb_x11_freeWindowClipBoxList(psRegions);
        
        psRegions = *ppWindowClipBoxList;
        		
        while(psRegions)
        {
            ui32NumRects++;
            psRegions = psRegions->next;
        }
    }
    else
    {
        *ppWindowClipBoxList = psb_x11_substractRects(display, NULL, &sRect);
        ui32NumRects = 1;
    }

    *pui32NumClipBoxList = ui32NumRects;

    return 0; 
}

VAStatus psb_PutSurface(
    VADriverContextP ctx,
    VASurfaceID surface,
    void *drawable, /* X Drawable */
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
    object_surface_p obj_surface;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int i32GrabXorgRet = 0;
    int bIsVisible = 0;
    psb_x11_win_t winRect;
    int i, ret;
    psb_x11_clip_list_t * pClipBoxList = NULL, * pClipNext = NULL;
    unsigned int          ui32NumClipBoxList = 0;
    VARectangle *         pVaWindowClipRects = NULL;

    Drawable draw = (Drawable)drawable;
    
    obj_surface = SURFACE(surface);
    if (NULL == obj_surface)
    {
        vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    if (output->dummy_putsurface) {
        psb__information_message("vaPutSurface: dummy mode, return directly\n");
        return VA_STATUS_SUCCESS;
    }

    if (output->output_method == PSB_PUTSURFACE_X11) {
        psb_putsurface_x11(ctx,surface,draw,srcx,srcy,srcw,srch,
                           destx,desty,destw,desth,flags);
        return VA_STATUS_SUCCESS;
    }

    if (output->fixed_fps > 0) {
        if ((tftarget.tv_sec == 0) && (tftarget.tv_usec == 0))
            gettimeofday(&tftarget,(struct timezone *)NULL);
        
        psb_doframerate(output->fixed_fps);
    }

    if(IS_MFLD(driver_data)) {
        i32GrabXorgRet = XGrabServer(ctx->native_dpy);
        ret = psb_x11_getWindowCoordinate(ctx->native_dpy, draw, &winRect, &bIsVisible);
        if (ret != 0)
        {
            if (i32GrabXorgRet != 0)
            {
                psb__error_message("%s: Failed to get X11 window coordinates error # %d\n", __func__, ret);
                XUngrabServer(ctx->native_dpy);
            }
            return VA_STATUS_ERROR_UNKNOWN;
        }

        if (!bIsVisible)
        {
            if (i32GrabXorgRet != 0)
            {
                XUngrabServer(ctx->native_dpy);
            }  
            return VA_STATUS_SUCCESS;
        }

        ret = psb_x11_createWindowClipBoxList(ctx->native_dpy, draw, &pClipBoxList, &ui32NumClipBoxList);
        if(ret != 0)
        {
            if (i32GrabXorgRet != 0)
            {
                psb__error_message("%s: get window clip boxes error # %d\n", __func__, ret);
                XUngrabServer(ctx->native_dpy);
            }
            return VA_STATUS_ERROR_UNKNOWN;
        }

        pVaWindowClipRects = (VARectangle *)malloc(sizeof(VARectangle)*ui32NumClipBoxList);
        if (!pVaWindowClipRects)
        {
            psb_x11_freeWindowClipBoxList(pClipBoxList);
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }
        
        memset(pVaWindowClipRects, 0, sizeof(VARectangle)*ui32NumClipBoxList);
        pClipNext = pClipBoxList;
#ifdef CLIP_DEBUG        
        psb__error_message("%s: Total %d clip boxes\n", __func__, ui32NumClipBoxList);
#endif
        for (i = 0; i < ui32NumClipBoxList; i++)
        {
            pVaWindowClipRects[i].x      = pClipNext->rect.i32Left;       
            pVaWindowClipRects[i].y      = pClipNext->rect.i32Top;
            pVaWindowClipRects[i].width  = pClipNext->rect.i32Right - pClipNext->rect.i32Left;
            pVaWindowClipRects[i].height = pClipNext->rect.i32Bottom - pClipNext->rect.i32Top;
#ifdef CLIP_DEBUG
            psb__error_message("%s: clip boxes Left Top (%d, %d) Right Bottom (%d, %d) width %d height %d\n", __func__,
                               pClipNext->rect.i32Left, pClipNext->rect.i32Top,
                               pClipNext->rect.i32Right, pClipNext->rect.i32Bottom,
                               pVaWindowClipRects[i].width, pVaWindowClipRects[i].height);
#endif
            pClipNext = pClipNext->next;
        }
#ifdef CLIP_DEBUG        
        psb__error_message("%s: dst (%d, %d) (%d, %d) width %d, height %d\n", __func__, 
                           winRect.i32Left, winRect.i32Top, 
                           winRect.i32Right, winRect.i32Bottom, 
                           winRect.ui32Width, winRect.ui32Height);
#endif
        vaStatus = psb_putsurface_overlay(ctx, surface, draw,
                                          srcx, srcy, srcw, srch,
                                          winRect.i32Left, winRect.i32Top, winRect.ui32Width, winRect.ui32Height,
                                          pVaWindowClipRects, ui32NumClipBoxList, flags); //cliprects, number_cliprects, flags);
        if (i32GrabXorgRet != 0)
        {
            XUngrabServer(ctx->native_dpy);
        }
        free(pVaWindowClipRects);
        psb_x11_freeWindowClipBoxList(pClipBoxList);
        return vaStatus;
    }
    else
    { 
        return psb_putsurface_xvideo(ctx, surface, draw,
                                     srcx, srcy, srcw, srch,
                                     destx, desty, destw, desth,
                                     cliprects, number_cliprects, flags);
    }
}
