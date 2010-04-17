#include <string.h> 
#include <stdlib.h> 

#include "lnc_ospm_event.h"

int
lnc_ospm_event_send(const char * obj_path, const char * method_name)
{
    int ret =0;
    
    if (obj_path == NULL || method_name == NULL)
    {
        psb__information_message("obj_path or method_name null pointer error\n");
        ret = 2;
        goto exit;
    }
    
    if (!strcmp(obj_path, "video_playback"))
    {
        if(!strcmp(method_name, OSPM_DBUS_EVENT_START)) 
        {
            ret = system("dbus-send --system --print-reply --dest=com.intel.mid.ospm \
                          /com/intel/mid/ospm/video_playback com.intel.mid.ospm.MMF.IndicateStart");
            if (ret < 0)
            {
                psb__information_message("ospm_event_send error: event %s send error #%d on %s\n", method_name, ret, obj_path);
            }
        } else if (!strcmp(method_name, OSPM_DBUS_EVENT_STOP))
        {
            ret = system("dbus-send --system --print-reply --dest=com.intel.mid.ospm \
                          /com/intel/mid/ospm/video_playback com.intel.mid.ospm.MMF.IndicateStop");
            if (ret < 0)
            {
                psb__information_message("ospm_event_send error: event %s send error #%d on %s\n", method_name, ret, obj_path);
            }
        } else
        {
            psb__information_message("ospm_event_send error: event %s isn't supported on %s\n", method_name, obj_path);
            ret = -1;
        }
    } else if (!strcmp(obj_path, "video_record"))
    {
        if(!strcmp(method_name, OSPM_DBUS_EVENT_START)) 
        {
            ret = system("dbus-send --system --print-reply --dest=com.intel.mid.ospm \
                          /com/intel/mid/ospm/video_record com.intel.mid.ospm.MMF.IndicateStart");
            if (ret < 0)
            {
                psb__information_message("ospm_event_send error: event %s send error #%d on %s\n", method_name, ret, obj_path);
            }
        } else if (!strcmp(method_name, OSPM_DBUS_EVENT_STOP))
        {
            ret = system("dbus-send --system --print-reply --dest=com.intel.mid.ospm \
                         /com/intel/mid/ospm/video_record com.intel.mid.ospm.MMF.IndicateStop");
            if (ret < 0)
            {
                psb__information_message("ospm_event_send error: event %s send error #%d on %s\n", method_name, ret, obj_path);
            }
        } else
        {
            psb__information_message("ospm_event_send error: event %s isn't supported on %s\n", method_name, obj_path);
            ret = 3;
        }
    } else
    {
        psb__information_message("ospm_event_send error: originator %s isn't supported\n", obj_path);
        ret = 4;
    }
exit:        
    return ret;
} 
