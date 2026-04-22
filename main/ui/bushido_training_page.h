#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BUSHIDO_TRAINING_PHASE_IDLE = 0,
    BUSHIDO_TRAINING_PHASE_COUNTDOWN,
    BUSHIDO_TRAINING_PHASE_GO,
    BUSHIDO_TRAINING_PHASE_WAITING_RANDOM,
    BUSHIDO_TRAINING_PHASE_UNLOCKED,
    BUSHIDO_TRAINING_PHASE_HIT_REGISTERED,
    BUSHIDO_TRAINING_PHASE_FALSE_START,
    BUSHIDO_TRAINING_PHASE_COMPLETED,
} bushido_training_phase_t;

typedef struct {
    const char *mode_label;          // es. "REACTION"
    const char *cue_label;           // es. "AUDIO+VISIVO"
    const char *hint_label;          // testo piccolo sotto stato
    const char *overlay_text;        // es. "3", "GO", "COLPISCI ORA"
    const char *subtitle;            // es. "Reaction Mode"

    bushido_training_phase_t phase;

    int timer_seconds;               // timer sessione o countdown
    int last_ms;                     // ultima reazione
    int best_ms;                     // migliore reazione
    int valid_hits;                  // colpi validi
    int false_starts;                // errori
    int target_current;              // progressivo
    int target_total;                // obiettivo totale

    bool start_enabled;
    bool pause_enabled;
    bool stop_enabled;
    bool overlay_visible;
} bushido_training_page_data_t;

typedef struct {
    void (*on_start)(void);
    void (*on_pause)(void);
    void (*on_stop)(void);
} bushido_training_page_callbacks_t;

lv_obj_t *bushido_training_page_create(lv_obj_t *parent);
void bushido_training_page_set_home(lv_obj_t *home);
void bushido_training_page_set_callbacks(const bushido_training_page_callbacks_t *callbacks);
void bushido_training_page_refresh(const bushido_training_page_data_t *data);
void bushido_training_page_show(void);
lv_obj_t *bushido_training_page_get_root(void);

#ifdef __cplusplus
}
#endif