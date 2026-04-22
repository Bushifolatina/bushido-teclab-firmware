#include "bushido_settings_page.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *g_settings_page = NULL;
static lv_obj_t *g_home_page = NULL;
static bushido_settings_page_callbacks_t g_callbacks = {0};

static lv_obj_t *lbl_title = NULL;
static lv_obj_t *lbl_subtitle = NULL;
static lv_obj_t *sw_audio = NULL;
static lv_obj_t *slider_volume = NULL;
static lv_obj_t *lbl_volume_value = NULL;
static lv_obj_t *slider_brightness = NULL;
static lv_obj_t *lbl_brightness_value = NULL;
static lv_obj_t *lbl_wifi_value = NULL;

static lv_obj_t *create_card(lv_obj_t *parent, const char *title,
                             lv_coord_t x, lv_coord_t y,
                             lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x121212), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0x505050), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_left(card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_right(card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(card, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(card, 8, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFF6A3D), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    return card;
}

static void set_value_label(lv_obj_t *label, int value)
{
    if (!label) return;
    lv_label_set_text_fmt(label, "%d", value);
}

static void back_btn_event_cb(lv_event_t *e)
{
    (void)e;
    if (g_home_page) {
        lv_scr_load(g_home_page);
    }
}

static void audio_switch_event_cb(lv_event_t *e)
{
    (void)e;
    if (!sw_audio) return;

    bool enabled = lv_obj_has_state(sw_audio, LV_STATE_CHECKED);

    if (slider_volume) {
        if (enabled) {
            lv_obj_clear_state(slider_volume, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(slider_volume, LV_STATE_DISABLED);
        }
    }

    if (g_callbacks.on_audio_enabled_changed) {
        g_callbacks.on_audio_enabled_changed(enabled);
    }
}

static void volume_slider_event_cb(lv_event_t *e)
{
    (void)e;
    if (!slider_volume) return;

    int value = lv_slider_get_value(slider_volume);
    set_value_label(lbl_volume_value, value);

    if (g_callbacks.on_audio_volume_changed) {
        g_callbacks.on_audio_volume_changed(value);
    }
}

static void brightness_slider_event_cb(lv_event_t *e)
{
    (void)e;
    if (!slider_brightness) return;

    int value = lv_slider_get_value(slider_brightness);
    set_value_label(lbl_brightness_value, value);

    if (g_callbacks.on_brightness_changed) {
        g_callbacks.on_brightness_changed(value);
    }
}

static void wifi_btn_event_cb(lv_event_t *e)
{
    (void)e;
    if (g_callbacks.on_open_wifi) {
        g_callbacks.on_open_wifi();
    }
}

static void reboot_btn_event_cb(lv_event_t *e)
{
    (void)e;
    if (g_callbacks.on_reboot) {
        g_callbacks.on_reboot();
    }
}

void bushido_settings_page_set_home(lv_obj_t *home)
{
    g_home_page = home;
}

void bushido_settings_page_set_callbacks(const bushido_settings_page_callbacks_t *callbacks)
{
    if (!callbacks) return;
    g_callbacks = *callbacks;
}

lv_obj_t *bushido_settings_page_create(lv_obj_t *parent)
{
    if (g_settings_page) {
        return g_settings_page;
    }

    g_settings_page = lv_obj_create(parent);
    lv_obj_set_size(g_settings_page, 480, 320);
    lv_obj_clear_flag(g_settings_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_settings_page, lv_color_hex(0x050505), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_settings_page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_settings_page, 0, LV_PART_MAIN);

    lbl_title = lv_label_create(g_settings_page);
    lv_label_set_text(lbl_title, "IMPOSTAZIONI");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_pos(lbl_title, 14, 10);

    lbl_subtitle = lv_label_create(g_settings_page);
    lv_label_set_text(lbl_subtitle, "Audio, display e rete");
    lv_obj_set_style_text_color(lbl_subtitle, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_subtitle, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_subtitle, 14, 38);

    lv_obj_t *btn_back = lv_btn_create(g_settings_page);
    lv_obj_set_size(btn_back, 92, 36);
    lv_obj_align(btn_back, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_radius(btn_back, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x91A6BF), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_back, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back_lbl = lv_label_create(btn_back);
    lv_label_set_text(btn_back_lbl, LV_SYMBOL_LEFT " HOME");
    lv_obj_center(btn_back_lbl);

    lv_obj_t *card_audio = create_card(g_settings_page, "AUDIO", 10, 70, 225, 120);

    lv_obj_t *lbl_audio = lv_label_create(card_audio);
    lv_label_set_text(lbl_audio, "Audio avvio");
    lv_obj_set_style_text_color(lbl_audio, lv_color_hex(0xD8D8D8), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_audio, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_audio, 0, 30);

    sw_audio = lv_switch_create(card_audio);
    lv_obj_set_pos(sw_audio, 120, 24);
    lv_obj_add_event_cb(sw_audio, audio_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *lbl_vol = lv_label_create(card_audio);
    lv_label_set_text(lbl_vol, "Volume");
    lv_obj_set_style_text_color(lbl_vol, lv_color_hex(0xD8D8D8), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_vol, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_vol, 0, 72);

    slider_volume = lv_slider_create(card_audio);
    lv_obj_set_size(slider_volume, 125, 8);
    lv_obj_set_pos(slider_volume, 68, 78);
    lv_slider_set_range(slider_volume, 0, 100);
    lv_obj_add_event_cb(slider_volume, volume_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lbl_volume_value = lv_label_create(card_audio);
    lv_label_set_text(lbl_volume_value, "80");
    lv_obj_set_style_text_color(lbl_volume_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_volume_value, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_volume_value, 168, 64);

    lv_obj_t *card_display = create_card(g_settings_page, "DISPLAY", 245, 70, 225, 120);

    lv_obj_t *lbl_bri = lv_label_create(card_display);
    lv_label_set_text(lbl_bri, "Luminosita");
    lv_obj_set_style_text_color(lbl_bri, lv_color_hex(0xD8D8D8), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_bri, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_bri, 0, 30);

    slider_brightness = lv_slider_create(card_display);
    lv_obj_set_size(slider_brightness, 125, 8);
    lv_obj_set_pos(slider_brightness, 68, 36);
    lv_slider_set_range(slider_brightness, 0, 100);
    lv_obj_add_event_cb(slider_brightness, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lbl_brightness_value = lv_label_create(card_display);
    lv_label_set_text(lbl_brightness_value, "80");
    lv_obj_set_style_text_color(lbl_brightness_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_brightness_value, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_brightness_value, 168, 22);

    lv_obj_t *lbl_wifi = lv_label_create(card_display);
    lv_label_set_text(lbl_wifi, "Wi-Fi");
    lv_obj_set_style_text_color(lbl_wifi, lv_color_hex(0xD8D8D8), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_wifi, 0, 72);

    lbl_wifi_value = lv_label_create(card_display);
    lv_label_set_text(lbl_wifi_value, "-");
    lv_obj_set_width(lbl_wifi_value, 120);
    lv_label_set_long_mode(lbl_wifi_value, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(lbl_wifi_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_wifi_value, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_wifi_value, 68, 72);

    lv_obj_t *card_actions = create_card(g_settings_page, "AZIONI", 10, 198, 460, 102);

    lv_obj_t *btn_wifi = lv_btn_create(card_actions);
    lv_obj_set_size(btn_wifi, 150, 42);
    lv_obj_set_pos(btn_wifi, 0, 36);
    lv_obj_set_style_radius(btn_wifi, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_wifi, lv_color_hex(0x1D5FA7), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_wifi, wifi_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_wifi_lbl = lv_label_create(btn_wifi);
    lv_label_set_text(btn_wifi_lbl, LV_SYMBOL_WIFI " WIFI");
    lv_obj_center(btn_wifi_lbl);

    lv_obj_t *btn_reboot = lv_btn_create(card_actions);
    lv_obj_set_size(btn_reboot, 150, 42);
    lv_obj_set_pos(btn_reboot, 165, 36);
    lv_obj_set_style_radius(btn_reboot, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_reboot, lv_color_hex(0x8A1E1E), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_reboot, reboot_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_reboot_lbl = lv_label_create(btn_reboot);
    lv_label_set_text(btn_reboot_lbl, LV_SYMBOL_REFRESH " RIAVVIA");
    lv_obj_center(btn_reboot_lbl);

    return g_settings_page;
}

void bushido_settings_page_refresh(const bushido_settings_page_data_t *data)
{
    if (!g_settings_page || !data) return;

    if (sw_audio) {
        if (data->audio_enabled) {
            lv_obj_add_state(sw_audio, LV_STATE_CHECKED);
            if (slider_volume) lv_obj_clear_state(slider_volume, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(sw_audio, LV_STATE_CHECKED);
            if (slider_volume) lv_obj_add_state(slider_volume, LV_STATE_DISABLED);
        }
    }

    if (slider_volume) {
        lv_slider_set_value(slider_volume, data->audio_volume, LV_ANIM_OFF);
    }
    set_value_label(lbl_volume_value, data->audio_volume);

    if (slider_brightness) {
        lv_slider_set_value(slider_brightness, data->brightness, LV_ANIM_OFF);
    }
    set_value_label(lbl_brightness_value, data->brightness);

    if (lbl_wifi_value) {
        lv_label_set_text(lbl_wifi_value, data->wifi_text ? data->wifi_text : "-");
    }
}

void bushido_settings_page_show(void)
{
    if (g_settings_page) {
        lv_scr_load(g_settings_page);
    }
}

lv_obj_t *bushido_settings_page_get_root(void)
{
    return g_settings_page;
}