#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *device_name;
    const char *device_id;
    const char *firmware_version;
    const char *hardware_version;

    const char *status_text;
    const char *wifi_text;
    int radio_channel;
    int paired_wrists;
    int max_wrists;

    int piezo_value;
    long loadcell_raw;
    long loadcell_offset;
    int total_hits;

    const char *fw_latest;
    const char *fw_update_text;
} bushido_device_page_data_t;

lv_obj_t *bushido_device_page_create(lv_obj_t *parent);
void bushido_device_page_show(void);
void bushido_device_page_refresh(const bushido_device_page_data_t *data);
lv_obj_t *bushido_device_page_get_root(void);
void bushido_device_page_set_home(lv_obj_t *home);

#ifdef __cplusplus
}
#endif