#ifndef _LNC_OSPM_EVENT_H_
#define _LNC_OSPM_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define OSPM_DBUS_MAX_OBJ_NAME_LEN  64

#define OSPM_EVENT_APP_LAUNCH         "launch"
#define OSPM_EVENT_APP_EXIT           "exit"
#define OSPM_EVENT_PPM_ACK            "ack"
#define OSPM_DBUS_EVENT_START         "start"
#define OSPM_DBUS_EVENT_STOP          "stop"
#define OSPM_DBUS_EVENT_PAUSE         "pause"
#define OSPM_DBUS_EVENT_ACTIVATED     "activate"
#define OSPM_DBUS_EVENT_DEACTIVATED   "deactivate"
#define OSPM_DBUS_EVENT_CHARGING      "charging"
#define OSPM_DBUS_EVENT_DISCHARGING   "discharging"
#define OSPM_DBUS_EVENT_NORMAL        "normal"
#define OSPM_DBUS_EVENT_ALERT         "alert"
#define OSPM_DBUS_EVENT_CRITICAL      "critical"

#ifdef ANDROID
 #include "psb_def.h"
#else
 void psb__information_message(const char *msg, ...);
#endif
int lnc_ospm_event_send(const char * obj_path, const char * method_name);

#ifdef __cplusplus
}
#endif

#endif /* _LNC_OSPM_EVENT_H_ */ 
