#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *subtitle;
    const char *status_title;
    const char *status_value;
    const char *status_hint;
} bushido_training_menu_page_data_t;

typedef struct {
    void (*on_reaction)(void);
} bushido_training_menu_page_callbacks_t;

lv_obj_t *bushido_training_menu_page_create(lv_obj_t *parent);
void bushido_training_menu_page_set_home(lv_obj_t *home);
void bushido_training_menu_page_set_callbacks(const bushido_training_menu_page_callbacks_t *callbacks);
void bushido_training_menu_page_refresh(const bushido_training_menu_page_data_t *data);
void bushido_training_menu_page_show(void);
lv_obj_t *bushido_training_menu_page_get_root(void);

#ifdef __cplusplus
}
#endif