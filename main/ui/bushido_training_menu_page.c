#include "bushido_training_menu_page.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *g_training_menu_page = NULL;
static lv_obj_t *g_home_page = NULL;
static bushido_training_menu_page_callbacks_t g_callbacks = {0};

static lv_obj_t *lbl_title = NULL;
static lv_obj_t *lbl_subtitle = NULL;

static lv_obj_t *lbl_status_title = NULL;
static lv_obj_t *lbl_status_value = NULL;
static lv_obj_t *lbl_status_hint = NULL;

static void set_status(const char *title, const char *value, const char *hint, lv_color_t color)
{
    if (lbl_status_title) {
        lv_label_set_text(lbl_status_title, title ? title : "STATO");
    }

    if (lbl_status_value) {
        lv_label_set_text(lbl_status_value, value ? value : "-");
        lv_obj_set_style_text_color(lbl_status_value, color, LV_PART_MAIN);
    }

    if (lbl_status_hint) {
        lv_label_set_text(lbl_status_hint, hint ? hint : "");
    }
}

static lv_obj_t *create_mode_btn(lv_obj_t *parent,
                                 const char *title,
                                 const char *subtitle,
                                 lv_coord_t x,
                                 lv_coord_t y,
                                 lv_coord_t w,
                                 lv_coord_t h,
                                 lv_color_t bg,
                                 lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_radius(btn, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl1 = lv_label_create(btn);
    lv_label_set_text(lbl1, title);
    lv_obj_set_style_text_color(lbl1, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(lbl1, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *lbl2 = lv_label_create(btn);
    lv_label_set_text(lbl2, subtitle);
    lv_obj_set_style_text_color(lbl2, lv_color_hex(0xE5E5E5), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lbl2, LV_ALIGN_CENTER, 0, 12);

    return btn;
}

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

static void back_btn_event_cb(lv_event_t *e)
{
    (void)e;
    if (g_home_page) {
        lv_scr_load(g_home_page);
    }
}

static void libero_btn_event_cb(lv_event_t *e)
{
    (void)e;
    set_status("LIBERO", "ATTIVO", "Apre la sessione libera", lv_color_hex(0x32D74B));

    if (g_callbacks.on_free) {
        g_callbacks.on_free();
    }
}

static void countdown_btn_event_cb(lv_event_t *e)
{
    (void)e;
    set_status("COUNTDOWN", "PRESTO", "Modalita non ancora attiva", lv_color_hex(0xFFD60A));
}

static void round_btn_event_cb(lv_event_t *e)
{
    (void)e;
    set_status("ROUND", "PRESTO", "Modalita non ancora attiva", lv_color_hex(0xFFD60A));
}

static void intervalli_btn_event_cb(lv_event_t *e)
{
    (void)e;
    set_status("INTERVALLI", "PRESTO", "Modalita non ancora attiva", lv_color_hex(0xFFD60A));
}

static void reaction_btn_event_cb(lv_event_t *e)
{
    (void)e;
    set_status("REACTION", "ATTIVO", "Apre la sessione reaction", lv_color_hex(0x32D74B));

    if (g_callbacks.on_reaction) {
        g_callbacks.on_reaction();
    }
}

void bushido_training_menu_page_set_home(lv_obj_t *home)
{
    g_home_page = home;
}

void bushido_training_menu_page_set_callbacks(const bushido_training_menu_page_callbacks_t *callbacks)
{
    if (!callbacks) return;
    g_callbacks = *callbacks;
}

lv_obj_t *bushido_training_menu_page_create(lv_obj_t *parent)
{
    if (g_training_menu_page) {
        return g_training_menu_page;
    }

    g_training_menu_page = lv_obj_create(parent);
    lv_obj_set_size(g_training_menu_page, 480, 320);
    lv_obj_clear_flag(g_training_menu_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_training_menu_page, lv_color_hex(0x050505), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_training_menu_page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_training_menu_page, 0, LV_PART_MAIN);

    lbl_title = lv_label_create(g_training_menu_page);
    lv_label_set_text(lbl_title, "ALLENAMENTI");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_pos(lbl_title, 14, 10);

    lbl_subtitle = lv_label_create(g_training_menu_page);
    lv_label_set_text(lbl_subtitle, "Scegli modalita");
    lv_obj_set_width(lbl_subtitle, 320);
    lv_label_set_long_mode(lbl_subtitle, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(lbl_subtitle, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_subtitle, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_subtitle, 14, 38);

    lv_obj_t *btn_back = lv_btn_create(g_training_menu_page);
    lv_obj_set_size(btn_back, 92, 36);
    lv_obj_align(btn_back, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_radius(btn_back, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x91A6BF), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_back, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back_lbl = lv_label_create(btn_back);
    lv_label_set_text(btn_back_lbl, LV_SYMBOL_LEFT " HOME");
    lv_obj_center(btn_back_lbl);

    create_mode_btn(g_training_menu_page, "LIBERO", "attivo", 10, 74, 145, 64,
                    lv_color_hex(0x4A5568), libero_btn_event_cb);

    create_mode_btn(g_training_menu_page, "COUNTDOWN", "presto", 167, 74, 145, 64,
                    lv_color_hex(0x5C4D7D), countdown_btn_event_cb);

    create_mode_btn(g_training_menu_page, "ROUND", "presto", 324, 74, 145, 64,
                    lv_color_hex(0x7D4D4D), round_btn_event_cb);

    create_mode_btn(g_training_menu_page, "INTERVALLI", "presto", 10, 148, 225, 64,
                    lv_color_hex(0x5A6E3A), intervalli_btn_event_cb);

    create_mode_btn(g_training_menu_page, "REACTION", "attivo", 245, 148, 225, 64,
                    lv_color_hex(0x1E7F37), reaction_btn_event_cb);

    lv_obj_t *status_card = create_card(g_training_menu_page, "STATO", 10, 222, 460, 88);

    lbl_status_title = lv_label_create(status_card);
    lv_label_set_text(lbl_status_title, "LIBERO");
    lv_obj_set_style_text_color(lbl_status_title, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_status_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_status_title, 0, 26);

    lbl_status_value = lv_label_create(status_card);
    lv_label_set_text(lbl_status_value, "ATTIVO");
    lv_obj_set_width(lbl_status_value, 430);
    lv_label_set_long_mode(lbl_status_value, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(lbl_status_value, lv_color_hex(0x32D74B), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_status_value, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(lbl_status_value, LV_ALIGN_CENTER, 0, -2);

    lbl_status_hint = lv_label_create(status_card);
    lv_label_set_text(lbl_status_hint, "Sessione libera disponibile");
    lv_obj_set_width(lbl_status_hint, 430);
    lv_label_set_long_mode(lbl_status_hint, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_status_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_status_hint, lv_color_hex(0xB0B0B0), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_status_hint, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lbl_status_hint, LV_ALIGN_BOTTOM_MID, 0, -4);

    return g_training_menu_page;
}

void bushido_training_menu_page_refresh(const bushido_training_menu_page_data_t *data)
{
    if (!g_training_menu_page || !data) return;

    if (lbl_subtitle) {
        lv_label_set_text(lbl_subtitle, (data->subtitle && data->subtitle[0]) ? data->subtitle : "Scegli modalita");
    }

    set_status(
        (data->status_title && data->status_title[0]) ? data->status_title : "LIBERO",
        (data->status_value && data->status_value[0]) ? data->status_value : "ATTIVO",
        (data->status_hint && data->status_hint[0]) ? data->status_hint : "Sessione libera disponibile",
        lv_color_hex(0x32D74B)
    );
}

void bushido_training_menu_page_show(void)
{
    if (g_training_menu_page) {
        lv_scr_load(g_training_menu_page);
    }
}

lv_obj_t *bushido_training_menu_page_get_root(void)
{
    return g_training_menu_page;
}