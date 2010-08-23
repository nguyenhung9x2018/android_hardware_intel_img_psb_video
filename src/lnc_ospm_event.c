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
