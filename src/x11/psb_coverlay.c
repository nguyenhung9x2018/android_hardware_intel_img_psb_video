/*
 * INTEL CONFIDENTIAL
 * Copyright 2007 Intel Corporation. All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material may contain trade secrets and
 * proprietary and confidential information of Intel Corporation and its
 * suppliers and licensors, and is protected by worldwide copyright and trade
 * secret laws and treaty provisions. No part of the Material may be used,
 * copied, reproduced, modified, published, uploaded, posted, transmitted,
 * distributed, or disclosed in any way without Intel's prior express written
 * permission. 
 * 
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <va/va_backend.h>
#include "psb_output.h"
#include "psb_surface.h"
#include "psb_buffer.h"
#include "psb_x11.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "psb_surface_ext.h"
#include <wsbm/wsbm_manager.h>

#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData
#define INIT_OUTPUT_PRIV    psb_x11_output_p output = (psb_x11_output_p)(((psb_driver_data_p)ctx->pDriverData)->ws_priv)
#define SURFACE(id)	((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))

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
	return 0;

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
    
    pCurrent = (psb_x11_clip_list_t *)calloc(1, sizeof(psb_x11_clip_list_t));
    if (pCurrent) {
        pCurrent->rect = *pRect;
        pCurrent->next = pClipNext;

        return pCurrent;
    } else
        return pClipNext;
}

static void
psb_x11_freeWindowClipBoxList(psb_x11_clip_list_t * pHead)
{
    psb_x11_clip_list_t * pNext = NULL;

    while (pHead) {
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
        psFirst->rect.i32Left = 0;
    else if(psFirst->rect.i32Left > display_width)
        psFirst->rect.i32Left = display_width;
    	
    if(psFirst->rect.i32Right < 0)
        psFirst->rect.i32Right = 0;
    else if(psFirst->rect.i32Right > display_width)
        psFirst->rect.i32Right = display_width;

    if(psFirst->rect.i32Top < 0)
        psFirst->rect.i32Top = 0;
    else if(psFirst->rect.i32Top> display_height)
        psFirst->rect.i32Top = display_height;
    	
    if(psFirst->rect.i32Bottom < 0)
        psFirst->rect.i32Bottom = 0;
    else if(psFirst->rect.i32Bottom > display_height)
        psFirst->rect.i32Bottom = display_height;
    
    while(psRegion) {
        psCur  = psFirst;
        psPrev = NULL;
        
        while(psCur) {
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
                     psPrev = psFirst;
                
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
                    psPrev = psFirst;
                
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
                    psPrev = psFirst;
                
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
                    psPrev = psFirst;
                
                psCur->rect.i32Bottom = psRegion->rect.i32Bottom;
            }
        
            if((IS_BETWEEN_RANGE(psRegion->rect.i32Left, psCur->rect.i32Left,   psRegion->rect.i32Right)) &&
               (IS_BETWEEN_RANGE(psRegion->rect.i32Left, psCur->rect.i32Right,  psRegion->rect.i32Right)) &&
               (IS_BETWEEN_RANGE(psRegion->rect.i32Top,  psCur->rect.i32Top,    psRegion->rect.i32Bottom)) &&
               (IS_BETWEEN_RANGE(psRegion->rect.i32Top,  psCur->rect.i32Bottom, psRegion->rect.i32Bottom)))
            {
                if(psPrev) {
                    psPrev->next = psCur->next;
                    free(psCur);
                    psCur = psPrev;
                } else {
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
    unsigned int i32NumChildren, i;
    int bIsVisible;
    unsigned int ui32NumRects = 0;
    psb_x11_clip_list_t *psRegions = NULL;
    psb_x11_win_t sRect;
    
    if (!display || (!ppWindowClipBoxList)||(!pui32NumClipBoxList))
        return -1;

    XResult = XQueryTree(display,
			 CurrentWindow,
			 &RootWindow,
			 &ParentWindow,
			 &pChildWindow,
		         &i32NumChildren);
    if(XResult == 0)
        return -2;

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
                XFree(pChildWindow);

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

static int psb_cleardrawable_stopoverlay(
    VADriverContextP ctx,
    Drawable draw, /* X Drawable */
    short destx,
    short desty,
    unsigned short destw,
    unsigned short desth
)
{
    INIT_DRIVER_DATA;
    INIT_OUTPUT_PRIV;
    
    XFillRectangle((Display *)ctx->native_dpy, draw, output->gc, destx, desty, destw, desth);
    XSync((Display *)ctx->native_dpy, False);
    
    XFreeGC((Display *)ctx->native_dpy, output->gc);
    output->gc = NULL;
    output->output_drawable = 0;
        
    XSync((Display *)ctx->native_dpy, False);

    psb_coverlay_stop(ctx);

    return 0;
}

static int last_num_clipbox = 0;
static VARectangle last_clipbox[16];

VAStatus psb_putsurface_coverlay(
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
    INIT_OUTPUT_PRIV;
    int i32GrabXorgRet = 0;
    int bIsVisible = 0;
    psb_x11_win_t winRect;
    int i, ret;
    psb_x11_clip_list_t * pClipBoxList = NULL, * pClipNext = NULL;
    unsigned int          ui32NumClipBoxList = 0;
    VARectangle *         pVaWindowClipRects = NULL;
    int x11_window_width, x11_window_height;

    if (flags & VA_CLEAR_DRAWABLE) {
        psb__information_message("Clean draw with color 0x%08x\n",driver_data->clear_color);
        psb_cleardrawable_stopoverlay(ctx, draw, destx, desty, destw, desth);
        
        return VA_STATUS_SUCCESS;
    }
    
    /* get window screen coordination */
    i32GrabXorgRet = XGrabServer(ctx->native_dpy);
    ret = psb_x11_getWindowCoordinate(ctx->native_dpy, draw, &winRect, &bIsVisible);
    if (ret != 0) {
        if (i32GrabXorgRet != 0) {
            psb__error_message("%s: Failed to get X11 window coordinates error # %d\n", __func__, ret);
            XUngrabServer(ctx->native_dpy);
        }
        return VA_STATUS_ERROR_UNKNOWN;
    }

    if (!bIsVisible) {
        if (i32GrabXorgRet != 0)
            XUngrabServer(ctx->native_dpy);
        return VA_STATUS_SUCCESS;
    }

    int display_width  = (int)(DisplayWidth(ctx->native_dpy, DefaultScreen(ctx->native_dpy))) - 1;
    int display_height = (int)(DisplayHeight(ctx->native_dpy, DefaultScreen(ctx->native_dpy))) - 1;
    
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
    
    /* get window clipbox */
    ret = psb_x11_createWindowClipBoxList(ctx->native_dpy, draw, &pClipBoxList, &ui32NumClipBoxList);
    if(ret != 0) {
        if (i32GrabXorgRet != 0) {
            psb__error_message("%s: get window clip boxes error # %d\n", __func__, ret);
            XUngrabServer(ctx->native_dpy);
        }
        return VA_STATUS_ERROR_UNKNOWN;
    }

    pVaWindowClipRects = (VARectangle *)calloc(1, sizeof(VARectangle)*ui32NumClipBoxList);
    if (!pVaWindowClipRects) {
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

    /* re-paint the color key if necessary */
    if (output->output_drawable != draw) {
        PsbPortPrivRec *pPriv = (PsbPortPrivPtr)(driver_data->ws_priv);
        
        output->output_drawable = draw;
        if (output->gc)
            XFreeGC((Display *)ctx->native_dpy, output->gc);
	output->gc = XCreateGC ((Display *)ctx->native_dpy, draw, 0, NULL);

        /* paint the color key */
        XSetForeground((Display *)ctx->native_dpy, output->gc, pPriv->colorKey);
        XFillRectangle((Display *)ctx->native_dpy, draw, output->gc, 0, 0, x11_window_width, x11_window_height);
        XSync((Display *)ctx->native_dpy, False);
    }

    if ((ui32NumClipBoxList != last_num_clipbox) ||
        (memcmp(&pVaWindowClipRects[0], &last_clipbox[0], (ui32NumClipBoxList > 16 ? 16: ui32NumClipBoxList)*sizeof(VARectangle))!=0)) {
        last_num_clipbox = ui32NumClipBoxList;
        memcpy(&last_clipbox[0], &pVaWindowClipRects[0],(ui32NumClipBoxList > 16 ? 16 : ui32NumClipBoxList)*sizeof(VARectangle));

        if (ui32NumClipBoxList <= 1) {
            XFillRectangle((Display *)ctx->native_dpy, draw, output->gc, 0, 0, x11_window_width, x11_window_height);
        } else {
            for (i = 0; i < ui32NumClipBoxList; i++) {
                psb__error_message("cliprect # %d @(%d, %d) width %d height %d\n", i, 
                                   last_clipbox[i].x, last_clipbox[i].y,
                                   last_clipbox[i].width, last_clipbox[i].height);
                XFillRectangle((Display *)ctx->native_dpy, draw, output->gc, 
                               last_clipbox[i].x - destx, last_clipbox[i].y - desty,
                               last_clipbox[i].width, last_clipbox[i].height);
            }
        }
        
        XSync((Display *)ctx->native_dpy, False);
    }

    /* display by overlay */
    psb_putsurface_overlay(
        ctx, surface, srcx, srcy, srcw, srch,
        /* screen coordinate */
        winRect.i32Left, winRect.i32Top,
        x11_window_width, x11_window_height,
        flags); 

    if (i32GrabXorgRet != 0)
        XUngrabServer(ctx->native_dpy);

    free(pVaWindowClipRects);
    psb_x11_freeWindowClipBoxList(pClipBoxList);

    return VA_STATUS_SUCCESS;
}
