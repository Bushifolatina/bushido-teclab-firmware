#include "bushido_training_page.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *g_training_page = NULL;
static lv_obj_t *g_home_page = NULL;
static bushido_training_page_callbacks_t g_callbacks = {0};

static lv_obj_t *lbl_title = NULL;
static lv_obj_t *lbl_subtitle = NULL;

static lv_obj_t *hero_card = NULL;
static lv_obj_t *lbl_state_title = NULL;
static lv_obj_t *lbl_state_value = NULL;
static lv_obj_t *lbl_hint = NULL;

static lv_obj_t *session_card = NULL;
static lv_obj_t *lbl_timer_title = NULL;
static lv_obj_t *lbl_timer_value = NULL;
static lv_obj_t *lbl_mode_title = NULL;
static lv_obj_t *lbl_mode_value = NULL;

static lv_obj_t *result_card = NULL;
static lv_obj_t *lbl_last_title = NULL;
static lv_obj_t *lbl_last_value = NULL;
static lv_obj_t *lbl_best_title = NULL;
static lv_obj_t *lbl_best_value = NULL;

static lv_obj_t *stats_card = NULL;
static lv_obj_t *lbl_valid_title = NULL;
static lv_obj_t *lbl_valid_value = NULL;
static lv_obj_t *lbl_false_title = NULL;
static lv_obj_t *lbl_false_value = NULL;
static lv_obj_t *lbl_target_title = NULL;
static lv_obj_t *lbl_target_value = NULL;
static lv_obj_t *lbl_cue_title = NULL;
static lv_obj_t *lbl_cue_value = NULL;

static lv_obj_t *btn_start = NULL;
static lv_obj_t *btn_pause = NULL;
static lv_obj_t *btn_stop = NULL;

static lv_obj_t *lbl_btn_start = NULL;
static lv_obj_t *lbl_btn_pause = NULL;
static lv_obj_t *lbl_btn_stop = NULL;

static lv_obj_t *overlay_label = NULL;

static const char *phase_to_main_label(bushido_training_phase_t phase)
{
    switch (phase) {
        case BUSHIDO_TRAINING_PHASE_COUNTDOWN:      return "COUNTDOWN";
        case BUSHIDO_TRAINING_PHASE_GO:             return "GO";
        case BUSHIDO_TRAINING_PHASE_WAITING_RANDOM: return "ATTENDI";
        case BUSHIDO_TRAINING_PHASE_UNLOCKED:       return "COLPISCI ORA";
        case BUSHIDO_TRAINING_PHASE_HIT_REGISTERED: return "COLPO VALIDO";
        case BUSHIDO_TRAINING_PHASE_FALSE_START:    return "TROPPO PRESTO";
        case BUSHIDO_TRAINING_PHASE_COMPLETED:      return "COMPLETATO";
        case BUSHIDO_TRAINING_PHASE_IDLE:
        default:                                    return "PRONTO";
    }
}

static lv_color_t phase_to_color(bushido_training_phase_t phase)
{
    switch (phase) {
        case BUSHIDO_TRAINING_PHASE_COUNTDOWN:      return lv_color_hex(0xFF9F0A);
        case BUSHIDO_TRAINING_PHASE_GO:             return lv_color_hex(0x30D5FF);
        case BUSHIDO_TRAINING_PHASE_WAITING_RANDOM: return lv_color_hex(0xFFD60A);
        case BUSHIDO_TRAINING_PHASE_UNLOCKED:       return lv_color_hex(0x32D74B);
        case BUSHIDO_TRAINING_PHASE_HIT_REGISTERED: return lv_color_hex(0x64D2FF);
        case BUSHIDO_TRAINING_PHASE_FALSE_START:    return lv_color_hex(0xFF453A);
        case BUSHIDO_TRAINING_PHASE_COMPLETED:      return lv_color_hex(0xBF5AF2);
        case BUSHIDO_TRAINING_PHASE_IDLE:
        default:                                    return lv_color_hex(0xFFFFFF);
    }
}

static void format_mmss(int total_seconds, char *out, size_t out_len)
{
    int mins = total_seconds / 60;
    int secs = total_seconds % 60;
    lv_snprintf(out, out_len, "%02d:%02d", mins, secs);
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

static void start_btn_event_cb(lv_event_t *e)
{
    (void)e;
    if (g_callbacks.on_start) {
        g_callbacks.on_start();
    }
}

static void pause_btn_event_cb(lv_event_t *e)
{
    (void)e;
    if (g_callbacks.on_pause) {
        g_callbacks.on_pause();
    }
}

static void stop_btn_event_cb(lv_event_t *e)
{
    (void)e;
    if (g_callbacks.on_stop) {
        g_callbacks.on_stop();
    }
}

static void set_button_enabled(lv_obj_t *btn, bool enabled)
{
    if (!btn) return;

    if (enabled) {
        lv_obj_clear_state(btn, LV_STATE_DISABLED);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
    }
}

void bushido_training_page_set_home(lv_obj_t *home)
{
    g_home_page = home;
}

void bushido_training_page_set_callbacks(const bushido_training_page_callbacks_t *callbacks)
{
    if (!callbacks) return;
    g_callbacks = *callbacks;
}

lv_obj_t *bushido_training_page_create(lv_obj_t *parent)
{
    if (g_training_page) {
        return g_training_page;
    }

    g_training_page = lv_obj_create(parent);
    lv_obj_set_size(g_training_page, 480, 320);
    lv_obj_clear_flag(g_training_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_training_page, lv_color_hex(0x050505), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_training_page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_training_page, 0, LV_PART_MAIN);

    lbl_title = lv_label_create(g_training_page);
    lv_label_set_text(lbl_title, "ALLENAMENTO");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_pos(lbl_title, 14, 10);

    lbl_subtitle = lv_label_create(g_training_page);
    lv_label_set_text(lbl_subtitle, "Reaction Mode");
    lv_obj_set_width(lbl_subtitle, 320);
    lv_label_set_long_mode(lbl_subtitle, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(lbl_subtitle, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_subtitle, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_subtitle, 14, 38);

    lv_obj_t *btn_back = lv_btn_create(g_training_page);
    lv_obj_set_size(btn_back, 92, 36);
    lv_obj_align(btn_back, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_radius(btn_back, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x91A6BF), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_back, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back_lbl = lv_label_create(btn_back);
    lv_label_set_text(btn_back_lbl, LV_SYMBOL_LEFT " HOME");
    lv_obj_center(btn_back_lbl);

    hero_card = create_card(g_training_page, "STATO", 10, 70, 460, 96);

    lbl_state_title = lv_label_create(hero_card);
    lv_label_set_text(lbl_state_title, "STATO");
    lv_obj_set_style_text_color(lbl_state_title, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_state_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lbl_state_title, LV_ALIGN_TOP_LEFT, 0, 24);

    lbl_state_value = lv_label_create(hero_card);
    lv_label_set_text(lbl_state_value, "PRONTO");
    lv_obj_set_width(lbl_state_value, 430);
    lv_label_set_long_mode(lbl_state_value, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_state_value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_state_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_state_value, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(lbl_state_value, LV_ALIGN_CENTER, 0, -4);

    lbl_hint = lv_label_create(hero_card);
    lv_label_set_text(lbl_hint, "Premi START");
    lv_obj_set_width(lbl_hint, 430);
    lv_label_set_long_mode(lbl_hint, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0xB0B0B0), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lbl_hint, LV_ALIGN_BOTTOM_MID, 0, -4);

    session_card = create_card(g_training_page, "SESSIONE", 10, 174, 225, 78);

    lbl_timer_title = lv_label_create(session_card);
    lv_label_set_text(lbl_timer_title, "TIMER");
    lv_obj_set_style_text_color(lbl_timer_title, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_timer_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_timer_title, 0, 26);

    lbl_timer_value = lv_label_create(session_card);
    lv_label_set_text(lbl_timer_value, "00:00");
    lv_obj_set_style_text_color(lbl_timer_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_timer_value, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_pos(lbl_timer_value, 80, 22);

    lbl_mode_title = lv_label_create(session_card);
    lv_label_set_text(lbl_mode_title, "MODO");
    lv_obj_set_style_text_color(lbl_mode_title, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_mode_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_mode_title, 0, 48);

    lbl_mode_value = lv_label_create(session_card);
    lv_label_set_text(lbl_mode_value, "REACTION");
    lv_obj_set_width(lbl_mode_value, 120);
    lv_label_set_long_mode(lbl_mode_value, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(lbl_mode_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_mode_value, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_mode_value, 80, 48);

    result_card = create_card(g_training_page, "RISULTATI", 245, 174, 225, 78);

    lbl_last_title = lv_label_create(result_card);
    lv_label_set_text(lbl_last_title, "ULTIMA");
    lv_obj_set_style_text_color(lbl_last_title, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_last_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_last_title, 0, 26);

    lbl_last_value = lv_label_create(result_card);
    lv_label_set_text(lbl_last_value, "-- ms");
    lv_obj_set_style_text_color(lbl_last_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_last_value, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_pos(lbl_last_value, 80, 22);

    lbl_best_title = lv_label_create(result_card);
    lv_label_set_text(lbl_best_title, "BEST");
    lv_obj_set_style_text_color(lbl_best_title, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_best_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_best_title, 0, 48);

    lbl_best_value = lv_label_create(result_card);
    lv_label_set_text(lbl_best_value, "-- ms");
    lv_obj_set_style_text_color(lbl_best_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_best_value, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_best_value, 80, 48);

    stats_card = create_card(g_training_page, "LIVE", 10, 258, 300, 52);

    lbl_valid_title = lv_label_create(stats_card);
    lv_label_set_text(lbl_valid_title, "VALIDI");
    lv_obj_set_style_text_color(lbl_valid_title, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_valid_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_valid_title, 0, 26);

    lbl_valid_value = lv_label_create(stats_card);
    lv_label_set_text(lbl_valid_value, "0");
    lv_obj_set_style_text_color(lbl_valid_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_valid_value, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_valid_value, 46, 24);

    lbl_false_title = lv_label_create(stats_card);
    lv_label_set_text(lbl_false_title, "ERRORI");
    lv_obj_set_style_text_color(lbl_false_title, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_false_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_false_title, 76, 26);

    lbl_false_value = lv_label_create(stats_card);
    lv_label_set_text(lbl_false_value, "0");
    lv_obj_set_style_text_color(lbl_false_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_false_value, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_false_value, 126, 24);

    lbl_target_title = lv_label_create(stats_card);
    lv_label_set_text(lbl_target_title, "TARGET");
    lv_obj_set_style_text_color(lbl_target_title, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_target_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_target_title, 156, 26);

    lbl_target_value = lv_label_create(stats_card);
    lv_label_set_text(lbl_target_value, "0/20");
    lv_obj_set_style_text_color(lbl_target_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_target_value, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_target_value, 205, 24);

    lbl_cue_title = lv_label_create(stats_card);
    lv_label_set_text(lbl_cue_title, "SEGNALE");
    lv_obj_set_style_text_color(lbl_cue_title, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_cue_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_cue_title, 250, 26);

    lbl_cue_value = lv_label_create(stats_card);
    lv_label_set_text(lbl_cue_value, "A+V");
    lv_obj_set_style_text_color(lbl_cue_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_cue_value, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(lbl_cue_value, 250, 10);

    btn_start = lv_btn_create(g_training_page);
    lv_obj_set_size(btn_start, 48, 52);
    lv_obj_set_pos(btn_start, 322, 258);
    lv_obj_set_style_radius(btn_start, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_start, lv_color_hex(0x1E7F37), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_start, start_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lbl_btn_start = lv_label_create(btn_start);
    lv_label_set_text(lbl_btn_start, LV_SYMBOL_PLAY);
    lv_obj_center(lbl_btn_start);

    btn_pause = lv_btn_create(g_training_page);
    lv_obj_set_size(btn_pause, 48, 52);
    lv_obj_set_pos(btn_pause, 376, 258);
    lv_obj_set_style_radius(btn_pause, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_pause, lv_color_hex(0x8C6D1F), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_pause, pause_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lbl_btn_pause = lv_label_create(btn_pause);
    lv_label_set_text(lbl_btn_pause, LV_SYMBOL_PAUSE);
    lv_obj_center(lbl_btn_pause);

    btn_stop = lv_btn_create(g_training_page);
    lv_obj_set_size(btn_stop, 48, 52);
    lv_obj_set_pos(btn_stop, 430, 258);
    lv_obj_set_style_radius(btn_stop, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_stop, lv_color_hex(0x8A1E1E), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_stop, stop_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lbl_btn_stop = lv_label_create(btn_stop);
    lv_label_set_text(lbl_btn_stop, LV_SYMBOL_STOP);
    lv_obj_center(lbl_btn_stop);

    overlay_label = lv_label_create(g_training_page);
    lv_label_set_text(overlay_label, "");
    lv_obj_set_width(overlay_label, 460);
    lv_label_set_long_mode(overlay_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(overlay_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(overlay_label, lv_color_hex(0x32D74B), LV_PART_MAIN);
    lv_obj_set_style_text_font(overlay_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(overlay_label, LV_ALIGN_CENTER, 0, -14);
    lv_obj_add_flag(overlay_label, LV_OBJ_FLAG_HIDDEN);

    return g_training_page;
}

void bushido_training_page_refresh(const bushido_training_page_data_t *data)
{
    if (!g_training_page || !data) return;

    char buf[64];
    lv_color_t phase_color = phase_to_color(data->phase);

    if (lbl_subtitle) {
        lv_label_set_text(lbl_subtitle, (data->subtitle && data->subtitle[0]) ? data->subtitle : "Reaction Mode");
    }

    if (lbl_state_value) {
        lv_label_set_text(lbl_state_value,
                          (data->overlay_text && data->phase == BUSHIDO_TRAINING_PHASE_COUNTDOWN && data->overlay_text[0])
                              ? data->overlay_text
                              : phase_to_main_label(data->phase));
        lv_obj_set_style_text_color(lbl_state_value, phase_color, LV_PART_MAIN);
    }

    if (lbl_hint) {
        lv_label_set_text(lbl_hint,
                          (data->hint_label && data->hint_label[0]) ? data->hint_label : "Premi START");
    }

    if (lbl_timer_value) {
        format_mmss((data->timer_seconds >= 0) ? data->timer_seconds : 0, buf, sizeof(buf));
        lv_label_set_text(lbl_timer_value, buf);
    }

    if (lbl_mode_value) {
        lv_label_set_text(lbl_mode_value,
                          (data->mode_label && data->mode_label[0]) ? data->mode_label : "REACTION");
    }

    if (lbl_last_value) {
        if (data->last_ms > 0) {
            lv_snprintf(buf, sizeof(buf), "%d ms", data->last_ms);
        } else {
            lv_snprintf(buf, sizeof(buf), "-- ms");
        }
        lv_label_set_text(lbl_last_value, buf);
    }

    if (lbl_best_value) {
        if (data->best_ms > 0) {
            lv_snprintf(buf, sizeof(buf), "%d ms", data->best_ms);
        } else {
            lv_snprintf(buf, sizeof(buf), "-- ms");
        }
        lv_label_set_text(lbl_best_value, buf);
    }

    if (lbl_valid_value) {
        lv_snprintf(buf, sizeof(buf), "%d", data->valid_hits);
        lv_label_set_text(lbl_valid_value, buf);
    }

    if (lbl_false_value) {
        lv_snprintf(buf, sizeof(buf), "%d", data->false_starts);
        lv_label_set_text(lbl_false_value, buf);
    }

    if (lbl_target_value) {
        lv_snprintf(buf, sizeof(buf), "%d/%d", data->target_current, data->target_total);
        lv_label_set_text(lbl_target_value, buf);
    }

    if (lbl_cue_value) {
        lv_label_set_text(lbl_cue_value,
                          (data->cue_label && data->cue_label[0]) ? data->cue_label : "AUDIO+VISIVO");
    }

    set_button_enabled(btn_start, data->start_enabled);
    set_button_enabled(btn_pause, data->pause_enabled);
    set_button_enabled(btn_stop, data->stop_enabled);

    if (overlay_label) {
        if (data->overlay_visible && data->overlay_text && data->overlay_text[0]) {
            lv_label_set_text(overlay_label, data->overlay_text);
            lv_obj_set_style_text_color(overlay_label, phase_color, LV_PART_MAIN);
            lv_obj_clear_flag(overlay_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(overlay_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void bushido_training_page_show(void)
{
    if (g_training_page) {
        lv_scr_load(g_training_page);
    }
}

lv_obj_t *bushido_training_page_get_root(void)
{
    return g_training_page;
}