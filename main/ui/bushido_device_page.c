#include "bushido_device_page.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *g_device_page = NULL;
static lv_obj_t *g_home_page = NULL;

static lv_obj_t *lbl_title = NULL;
static lv_obj_t *lbl_subtitle = NULL;

static lv_obj_t *lbl_status_value = NULL;
static lv_obj_t *lbl_wifi_value = NULL;
static lv_obj_t *lbl_radio_value = NULL;
static lv_obj_t *lbl_wrists_value = NULL;

static lv_obj_t *lbl_piezo_value = NULL;
static lv_obj_t *lbl_raw_value = NULL;
static lv_obj_t *lbl_offset_value = NULL;
static lv_obj_t *lbl_hits_value = NULL;

static lv_obj_t *lbl_fw_current_value = NULL;
static lv_obj_t *lbl_fw_latest_value = NULL;
static lv_obj_t *lbl_fw_update_value = NULL;

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

static void create_value_row_compact(lv_obj_t *parent,
                                     const char *label_text,
                                     lv_obj_t **value_out,
                                     lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, label_text);
    lv_obj_set_width(label, 102);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, lv_color_hex(0xD8D8D8), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(label, 0, y);

    lv_obj_t *value = lv_label_create(parent);
    lv_label_set_text(value, "-");
    lv_obj_set_width(value, 92);
    lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(value, 110, y);

    *value_out = value;
}

static void create_value_row_wide(lv_obj_t *parent,
                                  const char *label_text,
                                  lv_obj_t **value_out,
                                  lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, label_text);
    lv_obj_set_width(label, 120);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, lv_color_hex(0xD8D8D8), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(label, 0, y);

    lv_obj_t *value = lv_label_create(parent);
    lv_label_set_text(value, "-");
    lv_obj_set_width(value, 295);
    lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(value, 132, y);

    *value_out = value;
}

static void back_btn_event_cb(lv_event_t *e)
{
    (void)e;
    if (g_home_page) {
        lv_scr_load(g_home_page);
    }
}

static void format_wifi_compact(const char *src, char *dst, size_t dst_len)
{
    if (!src || !*src) {
        lv_snprintf(dst, dst_len, "-");
        return;
    }

    if (strstr(src, "RETE NON CONFIGURATA")) {
        lv_snprintf(dst, dst_len, "NON CONFIG.");
        return;
    }

    if (strstr(src, "DISCONNESSO")) {
        lv_snprintf(dst, dst_len, "DISCONNESSO");
        return;
    }

    if (strstr(src, "CONNESSO")) {
        const char *p1 = strchr(src, '(');
        const char *p2 = p1 ? strchr(p1, ')') : NULL;

        if (p1 && p2 && p2 > p1) {
            char ssid[48];
            size_t len = (size_t)(p2 - p1 - 1);
            if (len >= sizeof(ssid)) len = sizeof(ssid) - 1;
            memcpy(ssid, p1 + 1, len);
            ssid[len] = '\0';
            lv_snprintf(dst, dst_len, "%s", ssid);
            return;
        }

        lv_snprintf(dst, dst_len, "CONNESSO");
        return;
    }

    if (strncmp(src, "WiFi:", 5) == 0) {
        src += 5;
        while (*src == ' ') src++;
    }

    lv_snprintf(dst, dst_len, "%s", src);
}

void bushido_device_page_set_home(lv_obj_t *home)
{
    g_home_page = home;
}

lv_obj_t *bushido_device_page_create(lv_obj_t *parent)
{
    if (g_device_page) {
        return g_device_page;
    }

    g_device_page = lv_obj_create(parent);
    lv_obj_set_size(g_device_page, 480, 320);
    lv_obj_clear_flag(g_device_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_device_page, lv_color_hex(0x050505), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_device_page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_device_page, 0, LV_PART_MAIN);

    lbl_title = lv_label_create(g_device_page);
    lv_label_set_text(lbl_title, "DISPOSITIVO");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_pos(lbl_title, 14, 10);

    lbl_subtitle = lv_label_create(g_device_page);
    lv_label_set_text(lbl_subtitle, "Bushido Pad Pro");
    lv_obj_set_width(lbl_subtitle, 350);
    lv_label_set_long_mode(lbl_subtitle, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(lbl_subtitle, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_subtitle, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_subtitle, 14, 38);

    lv_obj_t *btn_back = lv_btn_create(g_device_page);
    lv_obj_set_size(btn_back, 92, 36);
    lv_obj_align(btn_back, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_radius(btn_back, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x91A6BF), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_back, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back_lbl = lv_label_create(btn_back);
    lv_label_set_text(btn_back_lbl, LV_SYMBOL_LEFT " HOME");
    lv_obj_center(btn_back_lbl);

    lv_obj_t *card_status = create_card(g_device_page, "STATO", 10, 70, 225, 120);
    create_value_row_compact(card_status, "Stato",        &lbl_status_value,  28);
    create_value_row_compact(card_status, "Wi-Fi",        &lbl_wifi_value,    50);
    create_value_row_compact(card_status, "Canale",       &lbl_radio_value,   72);
    create_value_row_compact(card_status, "Speed-Track",  &lbl_wrists_value,  94);

    lv_obj_t *card_sensors = create_card(g_device_page, "SENSORI", 245, 70, 225, 120);
    create_value_row_compact(card_sensors, "Sensore colpo", &lbl_piezo_value,  28);
    create_value_row_compact(card_sensors, "Sensore Forza", &lbl_raw_value,    50);
    create_value_row_compact(card_sensors, "Offset",        &lbl_offset_value, 72);
    create_value_row_compact(card_sensors, "Hits",          &lbl_hits_value,   94);

    lv_obj_t *card_fw = create_card(g_device_page, "FIRMWARE", 10, 198, 460, 102);
    create_value_row_wide(card_fw, "Attuale",       &lbl_fw_current_value, 24);
    create_value_row_wide(card_fw, "Disponibile",   &lbl_fw_latest_value,  49);
    create_value_row_wide(card_fw, "Aggiornamento", &lbl_fw_update_value,  74);

    return g_device_page;
}

void bushido_device_page_show(void)
{
    if (g_device_page) {
        lv_scr_load(g_device_page);
    }
}

void bushido_device_page_refresh(const bushido_device_page_data_t *data)
{
    if (!g_device_page || !data) return;

    char buf[160];
    char wifi_buf[64];

    if (lbl_title) {
        lv_label_set_text(lbl_title, "DISPOSITIVO");
    }

    if (lbl_subtitle) {
        lv_snprintf(buf, sizeof(buf), "%s | %s | HW %s",
                    data->device_name ? data->device_name : "Bushido Pad Pro",
                    data->device_id ? data->device_id : "-",
                    data->hardware_version ? data->hardware_version : "-");
        lv_label_set_text(lbl_subtitle, buf);
    }

    if (lbl_status_value) {
        lv_label_set_text(lbl_status_value, data->status_text ? data->status_text : "-");
    }

    if (lbl_wifi_value) {
        format_wifi_compact(data->wifi_text, wifi_buf, sizeof(wifi_buf));
        lv_label_set_text(lbl_wifi_value, wifi_buf);
    }

    if (lbl_radio_value) {
        lv_snprintf(buf, sizeof(buf), "%d", data->radio_channel);
        lv_label_set_text(lbl_radio_value, buf);
    }

    if (lbl_wrists_value) {
        lv_snprintf(buf, sizeof(buf), "%d/%d", data->paired_wrists, data->max_wrists);
        lv_label_set_text(lbl_wrists_value, buf);
    }

    if (lbl_piezo_value) {
        lv_snprintf(buf, sizeof(buf), "%d", data->piezo_value);
        lv_label_set_text(lbl_piezo_value, buf);
    }

    if (lbl_raw_value) {
        lv_snprintf(buf, sizeof(buf), "%ld", data->loadcell_raw);
        lv_label_set_text(lbl_raw_value, buf);
    }

    if (lbl_offset_value) {
        lv_snprintf(buf, sizeof(buf), "%ld", data->loadcell_offset);
        lv_label_set_text(lbl_offset_value, buf);
    }

    if (lbl_hits_value) {
        lv_snprintf(buf, sizeof(buf), "%d", data->total_hits);
        lv_label_set_text(lbl_hits_value, buf);
    }

    if (lbl_fw_current_value) {
        lv_label_set_text(lbl_fw_current_value, data->firmware_version ? data->firmware_version : "-");
    }

    if (lbl_fw_latest_value) {
        lv_label_set_text(lbl_fw_latest_value, data->fw_latest ? data->fw_latest : "-");
    }

    if (lbl_fw_update_value) {
        lv_label_set_text(lbl_fw_update_value, data->fw_update_text ? data->fw_update_text : "-");
    }
}

lv_obj_t *bushido_device_page_get_root(void)
{
    return g_device_page;
}