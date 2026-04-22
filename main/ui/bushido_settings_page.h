#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool audio_enabled;
    int audio_volume;     // 0..100
    int brightness;       // 0..100
    const char *wifi_text;
} bushido_settings_page_data_t;

typedef struct {
    void (*on_audio_enabled_changed)(bool enabled);
    void (*on_audio_volume_changed)(int volume);
    void (*on_brightness_changed)(int brightness);
    void (*on_open_wifi)(void);
    void (*on_reboot)(void);
} bushido_settings_page_callbacks_t;

lv_obj_t *bushido_settings_page_create(lv_obj_t *parent);
void bushido_settings_page_set_home(lv_obj_t *home);
void bushido_settings_page_set_callbacks(const bushido_settings_page_callbacks_t *callbacks);
void bushido_settings_page_refresh(const bushido_settings_page_data_t *data);
void bushido_settings_page_show(void);
lv_obj_t *bushido_settings_page_get_root(void);

#ifdef __cplusplus
}
#endif