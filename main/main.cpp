#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "ui/ui.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_rom_sys.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_io_expander_tca9554.h"
#include "esp_log.h"
#include "esp_codec_dev.h"

#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "lvgl.h"
#include "esp_lvgl_port.h"

#include "esp_axp2101_port.h"
#include "esp_camera_port.h"
#include "esp_es8311_port.h"
#include "esp_pcf85063_port.h"
#include "esp_qmi8658_port.h"
#include "esp_sdcard_port.h"
#include "esp_3inch5_lcd_port.h"
#include "ui/bushido_device_page.h"
#include "ui/screens/ui_HOME.h"
#include "ui/bushido_settings_page.h"
#include "ui/bushido_training_page.h"
#include "ui/bushido_training_menu_page.h"

extern const uint8_t boot_sound_wav_start[] asm("_binary_boot_sound_wav_start");
extern const uint8_t boot_sound_wav_end[]   asm("_binary_boot_sound_wav_end");

#define EXAMPLE_PIN_I2C_SDA GPIO_NUM_8
#define EXAMPLE_PIN_I2C_SCL GPIO_NUM_7
#define EXAMPLE_PIN_BUTTON  GPIO_NUM_0

#define EXAMPLE_DISPLAY_ROTATION 90

#if EXAMPLE_DISPLAY_ROTATION == 90 || EXAMPLE_DISPLAY_ROTATION == 270
#define EXAMPLE_LCD_H_RES 480
#define EXAMPLE_LCD_V_RES 320
#else
#define EXAMPLE_LCD_H_RES 320
#define EXAMPLE_LCD_V_RES 480
#endif

#define LCD_BUFFER_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES / 8)
#define I2C_PORT_NUM 0

static const char *TAG = "bushido_power_pad";

// ======================================================
// IDENTITA'
// ======================================================
static const char *BUSHIDO_DEVICE_ID = "COLP-0001";
static const char *BUSHIDO_FIRMWARE_VERSION = "1.6.0-MULTIWRIST_DYNAMIC";
static const char *BUSHIDO_HARDWARE_VERSION = "revA";

// ======================================================
// WIFI / BACKEND
// ======================================================
static char bushido_ssid[33] = "";
static char bushido_pass[64] = "";
static const char *BUSHIDO_BACKEND_HIT_URL = "https://api.teclab.bushidolatina.com/device/hit";

static bool wifi_connecting = false;
static bool wifi_connected = false;
static char wifi_status_text[64] = "WiFi: INIZIALIZZAZIONE...";

// ======================================================
// SENSORI PAD
// ======================================================
#define BUSHIDO_SENSOR_GPIO              GPIO_NUM_11
#define BUSHIDO_SENSOR_ADC_UNIT          ADC_UNIT_2
#define BUSHIDO_SENSOR_ADC_CHANNEL       ADC_CHANNEL_0
#define BUSHIDO_HIT_COOLDOWN_MS          200
#define BUSHIDO_DEBUG_INTERVAL_MS        1000

#define BUSHIDO_HX711_DATA_GPIO          GPIO_NUM_21
#define BUSHIDO_HX711_CLK_GPIO           GPIO_NUM_38

// ======================================================
// LOADCELL = FORZA PRINCIPALE
// ======================================================
#define BUSHIDO_ESPNOW_FALLBACK_CHANNEL  4
#define BUSHIDO_LOADCELL_HIT_THRESHOLD   9000
#define BUSHIDO_LOADCELL_RELEASE_TH      2500
#define BUSHIDO_FORCE_MAX_N              5000

// ======================================================
// FUSION WRIST
// ======================================================
#define BUSHIDO_FUSION_WINDOW_MS         1000
#define BUSHIDO_FUSION_DELAY_MS          40
#define BUSHIDO_MAX_WRISTS               4

// ======================================================
// HANDLE HW
// ======================================================
i2c_master_bus_handle_t i2c_bus_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;
esp_io_expander_handle_t expander_handle = NULL;
esp_lcd_touch_handle_t touch_handle = NULL;

extern "C" {
    LV_IMG_DECLARE(logoteclab);
}

lv_display_t *lvgl_disp = NULL;
lv_indev_t *lvgl_touch_indev = NULL;
static adc_oneshot_unit_handle_t adc_handle = NULL;

// ======================================================
// UI
// ======================================================
static lv_obj_t *title_label = NULL;
static lv_obj_t *wifi_label = NULL;
static lv_obj_t *wrist_label = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *hits_label = NULL;
static lv_obj_t *force_label = NULL;
static lv_obj_t *sensor_label = NULL;
static lv_obj_t *loadcell_label = NULL;
static lv_obj_t *device_label = NULL;

static lv_obj_t *wifi_modal = NULL;
static lv_obj_t *ta_pass = NULL;
static lv_obj_t *dd_ssid = NULL;
static lv_obj_t *kb = NULL;
static lv_obj_t *bushido_diag_panel = NULL;
static lv_obj_t *bushido_audio_icon = NULL;

// ======================================================
// STATO RUNTIME PAD
// ======================================================
typedef enum {
    BUSHIDO_TRAINING_MODE_FREE = 0,
    BUSHIDO_TRAINING_MODE_REACTION
} bushido_training_mode_t;

static bool bushido_sq_ui_initialized = false;
static bool bushido_sq_home_bound = false;
static bool bushido_audio_enabled = true;
static bool bushido_hit_active = false;
static int bushido_total_hits = 0;
static unsigned long bushido_last_hit_ms = 0;
static unsigned long bushido_last_debug_ms = 0;
static long bushido_last_loadcell_raw = 0;
static long bushido_loadcell_offset = 0;
static int bushido_audio_volume = 80;
static int bushido_display_brightness = 80;

static bushido_training_mode_t bushido_training_mode = BUSHIDO_TRAINING_MODE_REACTION;
static bushido_training_phase_t bushido_training_phase = BUSHIDO_TRAINING_PHASE_IDLE;
static int bushido_training_timer_seconds = 0;
static int bushido_training_last_ms = 0;
static int bushido_training_best_ms = 0;
static int bushido_training_valid_hits = 0;
static int bushido_training_false_starts = 0;
static int bushido_training_target_current = 0;
static int bushido_training_target_total = 20;
static bool bushido_training_running = false;
static TaskHandle_t bushido_training_task_handle = NULL;

static bool bushido_training_paused = false;
static unsigned long bushido_training_started_at_ms = 0;
static unsigned long bushido_training_phase_started_ms = 0;
static unsigned long bushido_training_paused_at_ms = 0;
static unsigned long bushido_training_unlocked_at_ms = 0;
static unsigned long bushido_training_random_delay_ms = 0;

static int bushido_training_countdown_value = 3;
static int bushido_training_random_delay_min_ms = 1500;
static int bushido_training_random_delay_max_ms = 5000;
static int bushido_training_allowed_reaction_window_ms = 1500;
static int bushido_training_false_start_penalty_ms = 2000;

// ======================================================
// RADIO / ESPNOW
// ======================================================
static uint8_t g_radio_channel = BUSHIDO_ESPNOW_FALLBACK_CHANNEL;

// ======================================================
// DATI WRIST / ESPNOW
// ======================================================
typedef struct __attribute__((packed)) wrist_data_t {
    char device_id[16];
    float acc_x;
    float acc_y;
    float acc_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    int angle_deg;
    char strike_type[16];
} wrist_data_t;

typedef struct __attribute__((packed)) pad_confirm_t {
    char type[16];
    char zone[16];
    int force_value;
} pad_confirm_t;

typedef struct __attribute__((packed)) wrist_pair_req_t {
    char type[16];
    char device_id[16];
} wrist_pair_req_t;

typedef struct __attribute__((packed)) pad_pair_ack_t {
    char type[16];
    char device_id[16];
    uint8_t channel;
    uint8_t slot_index;
    uint8_t reserved[2];
} pad_pair_ack_t;

typedef struct {
    bool occupied;
    bool pair_ack_sent;
    uint8_t mac[6];
    char device_id[16];
    float last_speed_raw;
    unsigned long last_speed_ms;
    int last_angle;
    char last_type[16];
} bushido_wrist_slot_t;

static bushido_wrist_slot_t g_wrist_slots[BUSHIDO_MAX_WRISTS];

// ======================================================
// CODA EVENTI HTTP
// ======================================================
typedef struct {
    int force_value;
    float speed_ms;
    int angle_deg;
    char strike_type[16];
    char zone[16];
    char wrist_device_id[16];
} bushido_hit_event_t;

static QueueHandle_t g_hit_queue = NULL;

// ======================================================
// FORWARD DECL
// ======================================================
void i2c_bus_init(void);
void io_expander_init(void);
void lv_port_init(void);
void bushido_ui_init(void);
void bushido_ui_show_ready(void);
void bushido_ui_show_hit(int force_value, float speed_value, int sensor_value);
void bushido_set_wifi_text(const char *text);
void bushido_set_wrist_text(const char *text);
void bushido_show_splash(void);
void bushido_sensor_init(void);
void bushido_hx711_init(void);
bool bushido_hx711_is_ready(void);
long bushido_hx711_read_raw(void);
void bushido_sensor_task(void *pvParameters);
void bushido_http_task(void *pvParameters);
void send_hit_to_backend(int force_value, float speed_value, int angle_deg, const char* strike_type, const char *zone_value);
void bushido_espnow_init(void);

static int bushido_loadcell_raw_to_force_n(long raw_abs);
static void bushido_refresh_radio_channel(void);
static esp_err_t bushido_register_wrist_peer(const uint8_t *mac);
static void bushido_send_hit_confirm_to_wrist_slot(int slot_index, int force_value, const char *zone_value);
static void bushido_send_pair_ack_to_wrist_slot(int slot_index);
static int bushido_find_wrist_slot_by_mac(const uint8_t *mac);
static int bushido_allocate_or_get_wrist_slot(const uint8_t *mac, const char *device_id);
static int bushido_select_best_wrist_slot(unsigned long hit_ts_ms);
static int bushido_count_paired_wrists(void);
static void bushido_bind_squareline_home(void);
static void bushido_wifi_tile_cb(lv_event_t * e);
static void bushido_audio_switch_cb(lv_event_t * e);
static void bushido_update_wrist_ui_summary(const char *last_type, const char *last_device_id, float last_speed);
static void bushido_open_device_page_cb(lv_event_t *e);
static void bushido_refresh_device_page_ui(int piezo_value);
static void bushido_open_settings_page_cb(lv_event_t *e);
static void bushido_refresh_settings_page_ui(void);
static void bushido_settings_audio_enabled_changed(bool enabled);
static void bushido_settings_audio_volume_changed(int volume);
static void bushido_settings_brightness_changed(int brightness);
static void bushido_settings_open_wifi(void);
static void bushido_settings_reboot(void);
static void open_wifi_manager_cb(lv_event_t * e);
static void bushido_open_training_page_cb(lv_event_t *e);
static void bushido_refresh_training_page_ui(void);
static void bushido_training_start(void);
static void bushido_training_pause(void);
static void bushido_training_stop(void);
static void bushido_open_training_menu_page_cb(lv_event_t *e);
static void bushido_training_menu_open_free(void);
static void bushido_training_menu_open_reaction(void);
static void bushido_training_task(void *pvParameters);
static bool bushido_training_handle_pad_hit(unsigned long now_ms);
static void bushido_training_enter_phase(bushido_training_phase_t phase, unsigned long now_ms);
static unsigned long bushido_training_random_between(unsigned long min_ms, unsigned long max_ms);

// ======================================================
// HELPER
// ======================================================
static long bushido_abs_long(long v)
{
    return (v < 0) ? -v : v;
}

static int bushido_loadcell_raw_to_force_n(long raw_abs)
{
    const long deadzone = 7000;
    const float scale = 0.020f;

    long effective = raw_abs - deadzone;
    if (effective < 0) {
        effective = 0;
    }

    int force_n = (int)lroundf((float)effective * scale);

    if (force_n < 0) {
        force_n = 0;
    }

    if (force_n > BUSHIDO_FORCE_MAX_N) {
        force_n = BUSHIDO_FORCE_MAX_N;
    }

    return force_n;
}

static void bushido_mac_to_string(const uint8_t *mac, char *buf, size_t buf_len)
{
    if (!mac || !buf || buf_len < 18) return;
    snprintf(buf, buf_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static const char *bushido_training_mode_label(void)
{
    return (bushido_training_mode == BUSHIDO_TRAINING_MODE_FREE) ? "LIBERO" : "REACTION";
}

static const char *bushido_training_mode_subtitle(void)
{
    return (bushido_training_mode == BUSHIDO_TRAINING_MODE_FREE) ? "Sessione libera" : "Reaction Mode";
}

static const char *bushido_training_mode_cue_label(void)
{
    return (bushido_training_mode == BUSHIDO_TRAINING_MODE_FREE) ? "PAD LIVE" : "AUDIO+VISIVO";
}

static const char *bushido_training_hint_for_phase(bushido_training_phase_t phase)
{
    switch (phase) {
        case BUSHIDO_TRAINING_PHASE_COUNTDOWN:
            return "Preparati";
        case BUSHIDO_TRAINING_PHASE_GO:
            return "Sessione avviata";
        case BUSHIDO_TRAINING_PHASE_WAITING_RANDOM:
            return "Attendi il segnale";
        case BUSHIDO_TRAINING_PHASE_UNLOCKED:
            return "Colpisci subito";
        case BUSHIDO_TRAINING_PHASE_HIT_REGISTERED:
            return "Reazione valida";
        case BUSHIDO_TRAINING_PHASE_FALSE_START:
            return "Colpo anticipato";
        case BUSHIDO_TRAINING_PHASE_COMPLETED:
            return "Sessione completata";
        case BUSHIDO_TRAINING_PHASE_IDLE:
        default:
            return "Premi START";
    }
}

static const char *bushido_training_current_hint(void)
{
    if (bushido_training_mode == BUSHIDO_TRAINING_MODE_FREE) {
        if (bushido_training_paused) return "In pausa";
        if (bushido_training_running) return "Colpisci liberamente";
        return "Premi START";
    }

    return bushido_training_paused ? "In pausa" : bushido_training_hint_for_phase(bushido_training_phase);
}

static unsigned long bushido_training_random_between(unsigned long min_ms, unsigned long max_ms)
{
    if (max_ms <= min_ms) return min_ms;
    return min_ms + (unsigned long)(esp_random() % (max_ms - min_ms + 1));
}

static void bushido_training_enter_phase(bushido_training_phase_t phase, unsigned long now_ms)
{
    bushido_training_phase = phase;
    bushido_training_phase_started_ms = now_ms;

    switch (phase) {
        case BUSHIDO_TRAINING_PHASE_COUNTDOWN:
            bushido_training_countdown_value = 3;
            break;

        case BUSHIDO_TRAINING_PHASE_WAITING_RANDOM:
            bushido_training_random_delay_ms = bushido_training_random_between(
                (unsigned long)bushido_training_random_delay_min_ms,
                (unsigned long)bushido_training_random_delay_max_ms
            );
            break;

        case BUSHIDO_TRAINING_PHASE_UNLOCKED:
            bushido_training_unlocked_at_ms = now_ms;
            break;

        default:
            break;
    }
}

static void bushido_refresh_training_page_ui(void)
{
    if (!bushido_training_page_get_root()) return;

    char overlay_buf[32] = "";

    if (bushido_training_mode == BUSHIDO_TRAINING_MODE_REACTION) {
        if (bushido_training_phase == BUSHIDO_TRAINING_PHASE_COUNTDOWN) {
            lv_snprintf(overlay_buf, sizeof(overlay_buf), "%d", bushido_training_countdown_value);
        } else if (bushido_training_phase == BUSHIDO_TRAINING_PHASE_GO) {
            lv_snprintf(overlay_buf, sizeof(overlay_buf), "GO");
        } else if (bushido_training_phase == BUSHIDO_TRAINING_PHASE_UNLOCKED) {
            lv_snprintf(overlay_buf, sizeof(overlay_buf), "COLPISCI ORA");
        } else if (bushido_training_phase == BUSHIDO_TRAINING_PHASE_FALSE_START) {
            lv_snprintf(overlay_buf, sizeof(overlay_buf), "TROPPO PRESTO");
        } else if (bushido_training_phase == BUSHIDO_TRAINING_PHASE_COMPLETED) {
            lv_snprintf(overlay_buf, sizeof(overlay_buf), "COMPLETATO");
        }
    }

    bushido_training_page_data_t data = {};
    data.mode_label = bushido_training_mode_label();
    data.cue_label = bushido_training_mode_cue_label();
    data.hint_label = bushido_training_current_hint();
    data.overlay_text = overlay_buf;
    data.subtitle = bushido_training_mode_subtitle();

    data.phase = bushido_training_phase;
    data.timer_seconds = bushido_training_timer_seconds;
    data.last_ms = bushido_training_last_ms;
    data.best_ms = bushido_training_best_ms;
    data.valid_hits = bushido_training_valid_hits;
    data.false_starts = bushido_training_false_starts;
    data.target_current = bushido_training_target_current;
    data.target_total = bushido_training_target_total;

    data.start_enabled = true;
    data.pause_enabled = bushido_training_running;
    data.stop_enabled = true;
    data.overlay_visible = (overlay_buf[0] != '\0');

    bushido_training_page_refresh(&data);
}

static void bushido_training_start(void)
{
    unsigned long now_ms = (unsigned long)esp_log_timestamp();

    if (bushido_training_paused) {
        unsigned long pause_delta = now_ms - bushido_training_paused_at_ms;
        bushido_training_started_at_ms += pause_delta;
        bushido_training_phase_started_ms += pause_delta;

        if (bushido_training_unlocked_at_ms > 0) {
            bushido_training_unlocked_at_ms += pause_delta;
        }

        bushido_training_paused = false;
        bushido_training_running = true;
        bushido_refresh_training_page_ui();
        return;
    }

    bushido_training_running = true;
    bushido_training_paused = false;
    bushido_training_timer_seconds = 0;
    bushido_training_last_ms = 0;
    bushido_training_best_ms = 0;
    bushido_training_valid_hits = 0;
    bushido_training_false_starts = 0;
    bushido_training_target_current = 0;
    bushido_training_target_total = (bushido_training_mode == BUSHIDO_TRAINING_MODE_FREE) ? 0 : 20;

    bushido_training_started_at_ms = now_ms;
    bushido_training_unlocked_at_ms = 0;

    if (bushido_training_mode == BUSHIDO_TRAINING_MODE_FREE) {
        bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_GO, now_ms);
    } else {
        bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_COUNTDOWN, now_ms);
    }

    bushido_refresh_training_page_ui();
}

static void bushido_training_pause(void)
{
    if (!bushido_training_running) return;

    bushido_training_running = false;
    bushido_training_paused = true;
    bushido_training_paused_at_ms = (unsigned long)esp_log_timestamp();
    bushido_refresh_training_page_ui();
}

static void bushido_training_stop(void)
{
    bushido_training_running = false;
    bushido_training_paused = false;
    bushido_training_phase = BUSHIDO_TRAINING_PHASE_IDLE;
    bushido_training_timer_seconds = 0;
    bushido_training_last_ms = 0;
    bushido_training_best_ms = 0;
    bushido_training_valid_hits = 0;
    bushido_training_false_starts = 0;
    bushido_training_target_current = 0;
    bushido_training_target_total = (bushido_training_mode == BUSHIDO_TRAINING_MODE_FREE) ? 0 : 20;
    bushido_training_countdown_value = 3;
    bushido_training_started_at_ms = 0;
    bushido_training_phase_started_ms = 0;
    bushido_training_unlocked_at_ms = 0;
    bushido_training_random_delay_ms = 0;

    bushido_refresh_training_page_ui();
}

static bool bushido_training_handle_pad_hit(unsigned long now_ms)
{
    if (!bushido_training_running || bushido_training_paused) {
        return false;
    }

    if (bushido_training_mode == BUSHIDO_TRAINING_MODE_FREE) {
        bushido_training_valid_hits++;
        bushido_training_target_current = bushido_training_valid_hits;
        bushido_training_phase = BUSHIDO_TRAINING_PHASE_GO;
        bushido_refresh_training_page_ui();
        return false;
    }

    if (bushido_training_phase == BUSHIDO_TRAINING_PHASE_UNLOCKED) {
        int reaction_ms = (int)(now_ms - bushido_training_unlocked_at_ms);

        if (reaction_ms <= bushido_training_allowed_reaction_window_ms) {
            bushido_training_last_ms = reaction_ms;
            bushido_training_valid_hits++;
            bushido_training_target_current++;

            if (bushido_training_best_ms == 0 || reaction_ms < bushido_training_best_ms) {
                bushido_training_best_ms = reaction_ms;
            }

            bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_HIT_REGISTERED, now_ms);
        } else {
            bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_WAITING_RANDOM, now_ms);
        }

        bushido_refresh_training_page_ui();
        return true;
    }

    if (bushido_training_phase == BUSHIDO_TRAINING_PHASE_COUNTDOWN ||
        bushido_training_phase == BUSHIDO_TRAINING_PHASE_GO ||
        bushido_training_phase == BUSHIDO_TRAINING_PHASE_WAITING_RANDOM) {
        bushido_training_false_starts++;
        bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_FALSE_START, now_ms);
        bushido_refresh_training_page_ui();
        return true;
    }

    return false;
}

static void bushido_training_task(void *pvParameters)
{
    (void)pvParameters;

    while (true) {
        if (bushido_training_running && !bushido_training_paused) {
            unsigned long now_ms = (unsigned long)esp_log_timestamp();

            if (bushido_training_started_at_ms > 0) {
                bushido_training_timer_seconds = (int)((now_ms - bushido_training_started_at_ms) / 1000UL);
            }

            if (bushido_training_mode == BUSHIDO_TRAINING_MODE_REACTION) {
                switch (bushido_training_phase) {
                    case BUSHIDO_TRAINING_PHASE_COUNTDOWN: {
                        unsigned long elapsed = now_ms - bushido_training_phase_started_ms;
                        int remaining = 3 - (int)(elapsed / 1000UL);

                        if (remaining < 1) remaining = 1;
                        bushido_training_countdown_value = remaining;

                        if (elapsed >= 3000UL) {
                            bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_GO, now_ms);
                        }
                        break;
                    }

                    case BUSHIDO_TRAINING_PHASE_GO:
                        if ((now_ms - bushido_training_phase_started_ms) >= 700UL) {
                            bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_WAITING_RANDOM, now_ms);
                        }
                        break;

                    case BUSHIDO_TRAINING_PHASE_WAITING_RANDOM:
                        if ((now_ms - bushido_training_phase_started_ms) >= bushido_training_random_delay_ms) {
                            bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_UNLOCKED, now_ms);
                        }
                        break;

                    case BUSHIDO_TRAINING_PHASE_UNLOCKED:
                        if ((now_ms - bushido_training_unlocked_at_ms) >= (unsigned long)bushido_training_allowed_reaction_window_ms) {
                            bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_WAITING_RANDOM, now_ms);
                        }
                        break;

                    case BUSHIDO_TRAINING_PHASE_HIT_REGISTERED:
                        if ((now_ms - bushido_training_phase_started_ms) >= 650UL) {
                            if (bushido_training_target_current >= bushido_training_target_total) {
                                bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_COMPLETED, now_ms);
                                bushido_training_running = false;
                            } else {
                                bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_WAITING_RANDOM, now_ms);
                            }
                        }
                        break;

                    case BUSHIDO_TRAINING_PHASE_FALSE_START:
                        if ((now_ms - bushido_training_phase_started_ms) >= (unsigned long)bushido_training_false_start_penalty_ms) {
                            bushido_training_enter_phase(BUSHIDO_TRAINING_PHASE_WAITING_RANDOM, now_ms);
                        }
                        break;

                    case BUSHIDO_TRAINING_PHASE_COMPLETED:
                        bushido_training_running = false;
                        break;

                    case BUSHIDO_TRAINING_PHASE_IDLE:
                    default:
                        break;
                }
            }

            if (lvgl_port_lock(0)) {
                bushido_refresh_training_page_ui();
                lvgl_port_unlock();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void bushido_open_training_page_cb(lv_event_t *e)
{
    (void)e;

    if (!bushido_training_page_get_root()) {
        bushido_training_page_callbacks_t callbacks = {};
        callbacks.on_start = bushido_training_start;
        callbacks.on_pause = bushido_training_pause;
        callbacks.on_stop = bushido_training_stop;

        bushido_training_page_create(NULL);
        bushido_training_page_set_home(ui_HOME);
        bushido_training_page_set_callbacks(&callbacks);
    }

    bushido_refresh_training_page_ui();
    bushido_training_page_show();
}

static void bushido_training_menu_open_free(void)
{
    bushido_training_mode = BUSHIDO_TRAINING_MODE_FREE;
    bushido_training_stop();
    bushido_open_training_page_cb(NULL);
}

static void bushido_training_menu_open_reaction(void)
{
    bushido_training_mode = BUSHIDO_TRAINING_MODE_REACTION;
    bushido_training_stop();
    bushido_open_training_page_cb(NULL);
}

static void bushido_open_training_menu_page_cb(lv_event_t *e)
{
    (void)e;

    if (!bushido_training_menu_page_get_root()) {
        bushido_training_menu_page_callbacks_t callbacks = {};
        callbacks.on_free = bushido_training_menu_open_free;
        callbacks.on_reaction = bushido_training_menu_open_reaction;

        bushido_training_menu_page_create(NULL);
        bushido_training_menu_page_set_home(ui_HOME);
        bushido_training_menu_page_set_callbacks(&callbacks);
    }

    bushido_training_menu_page_data_t data = {};
    data.subtitle = "Scegli modalita";
    data.status_title = "MENU";
    data.status_value = "ATTIVO";
    data.status_hint = "Libero e Reaction disponibili";

    bushido_training_menu_page_refresh(&data);
    bushido_training_menu_page_show();
}

static void bushido_settings_audio_enabled_changed(bool enabled)
{
    bushido_audio_enabled = enabled;
}

static void bushido_settings_audio_volume_changed(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    bushido_audio_volume = volume;
}

static void bushido_settings_brightness_changed(int brightness)
{
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;

    bushido_display_brightness = brightness;
    esp_3inch5_brightness_port_set(bushido_display_brightness);
}

static void bushido_settings_open_wifi(void)
{
    open_wifi_manager_cb(NULL);
}

static void bushido_settings_reboot(void)
{
    esp_restart();
}

static void bushido_refresh_settings_page_ui(void)
{
    if (!bushido_settings_page_get_root()) return;

    bushido_settings_page_data_t data = {};
    data.audio_enabled = bushido_audio_enabled;
    data.audio_volume = bushido_audio_volume;
    data.brightness = bushido_display_brightness;
    data.wifi_text = wifi_status_text;

    bushido_settings_page_refresh(&data);
}

static void bushido_open_settings_page_cb(lv_event_t *e)
{
    (void)e;

    if (!bushido_settings_page_get_root()) {
        bushido_settings_page_callbacks_t callbacks = {};
        callbacks.on_audio_enabled_changed = bushido_settings_audio_enabled_changed;
        callbacks.on_audio_volume_changed = bushido_settings_audio_volume_changed;
        callbacks.on_brightness_changed = bushido_settings_brightness_changed;
        callbacks.on_open_wifi = bushido_settings_open_wifi;
        callbacks.on_reboot = bushido_settings_reboot;

        bushido_settings_page_create(NULL);
        bushido_settings_page_set_home(ui_HOME);
        bushido_settings_page_set_callbacks(&callbacks);
    }

    bushido_refresh_settings_page_ui();
    bushido_settings_page_show();
}

static void bushido_refresh_radio_channel(void)
{
    wifi_ap_record_t ap_info;
    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK && ap_info.primary > 0) {
        g_radio_channel = ap_info.primary;
        return;
    }

    if (esp_wifi_get_channel(&primary, &second) == ESP_OK && primary > 0) {
        g_radio_channel = primary;
        return;
    }

    g_radio_channel = BUSHIDO_ESPNOW_FALLBACK_CHANNEL;
}

static int bushido_count_paired_wrists(void)
{
    int count = 0;
    for (int i = 0; i < BUSHIDO_MAX_WRISTS; i++) {
        if (g_wrist_slots[i].occupied) {
            count++;
        }
    }
    return count;
}

static void bushido_update_wrist_ui_summary(const char *last_type, const char *last_device_id, float last_speed)
{
    if (!wrist_label) return;
    if (!lvgl_port_lock(0)) return;

    char buffer[96];
    int paired = bushido_count_paired_wrists();

    if (last_device_id && strlen(last_device_id) > 0) {
        snprintf(buffer, sizeof(buffer), "W:%d/%d %s %.1f", paired, BUSHIDO_MAX_WRISTS, last_device_id, last_speed);
    } else {
        snprintf(buffer, sizeof(buffer), "W:%d/%d in attesa", paired, BUSHIDO_MAX_WRISTS);
    }

    lv_label_set_text(wrist_label, buffer);
    lvgl_port_unlock();
}

static int bushido_find_wrist_slot_by_mac(const uint8_t *mac)
{
    if (!mac) return -1;

    for (int i = 0; i < BUSHIDO_MAX_WRISTS; i++) {
        if (g_wrist_slots[i].occupied && memcmp(g_wrist_slots[i].mac, mac, 6) == 0) {
            return i;
        }
    }
    return -1;
}

static int bushido_allocate_or_get_wrist_slot(const uint8_t *mac, const char *device_id)
{
    int existing = bushido_find_wrist_slot_by_mac(mac);
    if (existing >= 0) {
        if (device_id && strlen(device_id) > 0) {
            strncpy(g_wrist_slots[existing].device_id, device_id, sizeof(g_wrist_slots[existing].device_id) - 1);
            g_wrist_slots[existing].device_id[sizeof(g_wrist_slots[existing].device_id) - 1] = '\0';
        }
        return existing;
    }

    for (int i = 0; i < BUSHIDO_MAX_WRISTS; i++) {
        if (!g_wrist_slots[i].occupied) {
            memset(&g_wrist_slots[i], 0, sizeof(g_wrist_slots[i]));
            g_wrist_slots[i].occupied = true;
            memcpy(g_wrist_slots[i].mac, mac, 6);

            if (device_id && strlen(device_id) > 0) {
                strncpy(g_wrist_slots[i].device_id, device_id, sizeof(g_wrist_slots[i].device_id) - 1);
                g_wrist_slots[i].device_id[sizeof(g_wrist_slots[i].device_id) - 1] = '\0';
            } else {
                strncpy(g_wrist_slots[i].device_id, "BRACC-UNK", sizeof(g_wrist_slots[i].device_id) - 1);
            }

            strncpy(g_wrist_slots[i].last_type, "-", sizeof(g_wrist_slots[i].last_type) - 1);
            return i;
        }
    }

    ESP_LOGW(TAG, "Massimo numero di wrist raggiunto (%d)", BUSHIDO_MAX_WRISTS);
    return -1;
}

static int bushido_select_best_wrist_slot(unsigned long hit_ts_ms)
{
    int best_slot = -1;
    unsigned long best_diff = 0xFFFFFFFFUL;

    for (int i = 0; i < BUSHIDO_MAX_WRISTS; i++) {
        if (!g_wrist_slots[i].occupied) continue;
        if (g_wrist_slots[i].last_speed_ms == 0) continue;

        unsigned long diff = (hit_ts_ms > g_wrist_slots[i].last_speed_ms)
            ? (hit_ts_ms - g_wrist_slots[i].last_speed_ms)
            : (g_wrist_slots[i].last_speed_ms - hit_ts_ms);

        if (diff <= BUSHIDO_FUSION_WINDOW_MS && diff < best_diff) {
            best_diff = diff;
            best_slot = i;
        }
    }

    return best_slot;
}

// ======================================================
// NVS WIFI
// ======================================================
void load_wifi_credentials() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return;

    size_t required_size = sizeof(bushido_ssid);
    err = nvs_get_str(my_handle, "ssid", bushido_ssid, &required_size);
    if (err != ESP_OK) strcpy(bushido_ssid, "");

    required_size = sizeof(bushido_pass);
    err = nvs_get_str(my_handle, "pass", bushido_pass, &required_size);
    if (err != ESP_OK) strcpy(bushido_pass, "");

    nvs_close(my_handle);
    ESP_LOGI(TAG, "Credenziali caricate - SSID: '%s'", bushido_ssid);
}

void save_wifi_credentials() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_str(my_handle, "ssid", bushido_ssid);
        nvs_set_str(my_handle, "pass", bushido_pass);
        nvs_commit(my_handle);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "Credenziali salvate in NVS");
    }
}

// ======================================================
// UI WIFI
// ======================================================
void update_wifi_ui_status(const char* message, lv_color_t color) {
    if (lvgl_port_lock(0)) {
        if (wifi_label) {
            lv_label_set_text(wifi_label, message);
            lv_obj_set_style_text_color(wifi_label, color, LV_PART_MAIN);
        }
        lvgl_port_unlock();
    }
}

static void close_modal_cb(lv_event_t * e) {
    if (wifi_modal) {
        lv_obj_del(wifi_modal);
        wifi_modal = NULL;
    }
}

static void scan_btn_cb(lv_event_t * e) {
    if (!dd_ssid) return;

    lv_dropdown_set_options(dd_ssid, "Scansione in corso...");

    wifi_scan_config_t scan_config = {};
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);

    if (err != ESP_OK) {
        lv_dropdown_set_options(dd_ssid, "Errore Scan");
        return;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    if (ap_count == 0) {
        lv_dropdown_set_options(dd_ssid, "Nessuna rete trovata");
        return;
    }

    wifi_ap_record_t *ap_info = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_info) {
        lv_dropdown_set_options(dd_ssid, "Memoria insufficiente");
        return;
    }

    esp_wifi_scan_get_ap_records(&ap_count, ap_info);

    char *options = (char *)malloc(1024);
    if (!options) {
        free(ap_info);
        lv_dropdown_set_options(dd_ssid, "Memoria insufficiente");
        return;
    }

    options[0] = '\0';
    int added = 0;
    for (int i = 0; i < ap_count && added < 20; i++) {
        if (strlen((char *)ap_info[i].ssid) > 0) {
            if (strlen(options) + strlen((char *)ap_info[i].ssid) + 2 < 1024) {
                strcat(options, (char *)ap_info[i].ssid);
                strcat(options, "\n");
                added++;
            }
        }
    }

    if (strlen(options) > 0) {
        options[strlen(options) - 1] = '\0';
        lv_dropdown_set_options(dd_ssid, options);
    } else {
        lv_dropdown_set_options(dd_ssid, "Reti nascoste o vuote");
    }

    free(options);
    free(ap_info);
}

static void connect_btn_cb(lv_event_t * e) {
    char ssid_buf[33];
    lv_dropdown_get_selected_str(dd_ssid, ssid_buf, sizeof(ssid_buf));
    const char * pass = lv_textarea_get_text(ta_pass);

    if (strlen(ssid_buf) == 0 || strcmp(ssid_buf, "Seleziona Rete...") == 0) {
        lv_textarea_set_text(ta_pass, "");
        lv_dropdown_set_options(dd_ssid, "Seleziona una rete valida");
        return;
    }

    strncpy(bushido_ssid, ssid_buf, sizeof(bushido_ssid) - 1);
    bushido_ssid[sizeof(bushido_ssid) - 1] = '\0';

    strncpy(bushido_pass, pass, sizeof(bushido_pass) - 1);
    bushido_pass[sizeof(bushido_pass) - 1] = '\0';

    save_wifi_credentials();

    if (wifi_modal) {
        lv_obj_del(wifi_modal);
        wifi_modal = NULL;
    }

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, bushido_ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, bushido_pass, sizeof(wifi_config.sta.password) - 1);

    if (strlen(bushido_pass) == 0) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        wifi_config.sta.pmf_cfg.capable = true;
        wifi_config.sta.pmf_cfg.required = false;
    }

    ESP_LOGW(TAG, "Cambio Rete -> SSID: '%s'", wifi_config.sta.ssid);

    esp_err_t err_dis = esp_wifi_disconnect();
    if (err_dis != ESP_OK && err_dis != ESP_ERR_WIFI_NOT_CONNECT) {
        ESP_LOGW(TAG, "esp_wifi_disconnect: %s", esp_err_to_name(err_dis));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    esp_err_t err_conn = esp_wifi_connect();
    if (err_conn != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err_conn));
        wifi_connecting = false;
        wifi_connected = false;
        snprintf(wifi_status_text, sizeof(wifi_status_text), "WiFi: ERRORE CONNESSIONE");
        update_wifi_ui_status(wifi_status_text, lv_color_hex(0xFF0000));
    } else {
        wifi_connecting = true;
        wifi_connected = false;
        snprintf(wifi_status_text, sizeof(wifi_status_text), "WiFi: Connessione a %s...", bushido_ssid);
        update_wifi_ui_status(wifi_status_text, lv_color_hex(0xFFFF00));
    }
}

static void bushido_open_device_page_cb(lv_event_t *e)
{
    (void)e;

    bushido_device_page_data_t data = {};
    data.device_name = "BUSHIDO PAD PRO";
    data.device_id = BUSHIDO_DEVICE_ID;
    data.firmware_version = BUSHIDO_FIRMWARE_VERSION;
    data.hardware_version = BUSHIDO_HARDWARE_VERSION;
    data.status_text = "ONLINE";
    data.wifi_text = wifi_status_text;
    data.radio_channel = g_radio_channel;
    data.paired_wrists = bushido_count_paired_wrists();
    data.max_wrists = BUSHIDO_MAX_WRISTS;
    data.piezo_value = 0;
    data.loadcell_raw = bushido_last_loadcell_raw;
    data.loadcell_offset = bushido_loadcell_offset;
    data.total_hits = bushido_total_hits;
    data.fw_latest = BUSHIDO_FIRMWARE_VERSION;
    data.fw_update_text = "manuale";

    if (!bushido_device_page_get_root()) {
        bushido_device_page_create(NULL);
        bushido_device_page_set_home(ui_HOME);
    }

    bushido_device_page_refresh(&data);
    bushido_device_page_show();
}

static void bushido_refresh_device_page_ui(int piezo_value)
{
    if (!bushido_device_page_get_root()) return;

    bushido_device_page_data_t data = {};
    data.device_name = "BUSHIDO PAD PRO";
    data.device_id = BUSHIDO_DEVICE_ID;
    data.firmware_version = BUSHIDO_FIRMWARE_VERSION;
    data.hardware_version = BUSHIDO_HARDWARE_VERSION;
    data.status_text = "ONLINE";
    data.wifi_text = wifi_status_text;
    data.radio_channel = g_radio_channel;
    data.paired_wrists = bushido_count_paired_wrists();
    data.max_wrists = BUSHIDO_MAX_WRISTS;
    data.piezo_value = piezo_value;
    data.loadcell_raw = bushido_last_loadcell_raw;
    data.loadcell_offset = bushido_loadcell_offset;
    data.total_hits = bushido_total_hits;
    data.fw_latest = BUSHIDO_FIRMWARE_VERSION;
    data.fw_update_text = "manuale";

    bushido_device_page_refresh(&data);
}

static void open_wifi_manager_cb(lv_event_t * e) {
    if (wifi_modal) return;

    wifi_modal = lv_obj_create(lv_scr_act());
    lv_obj_set_size(wifi_modal, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    lv_obj_center(wifi_modal);
    lv_obj_set_style_bg_color(wifi_modal, lv_color_hex(0x151515), 0);
    lv_obj_set_style_border_width(wifi_modal, 0, 0);

    lv_obj_t * title = lv_label_create(wifi_modal);
    lv_label_set_text(title, "IMPOSTAZIONI RETE WIFI");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

    dd_ssid = lv_dropdown_create(wifi_modal);
    lv_obj_set_width(dd_ssid, 220);
    lv_obj_align(dd_ssid, LV_ALIGN_TOP_LEFT, 10, 35);

    if (strlen(bushido_ssid) > 0) {
        lv_dropdown_set_options(dd_ssid, bushido_ssid);
    } else {
        lv_dropdown_set_options(dd_ssid, "Seleziona Rete...");
    }

    lv_obj_t * btn_scan = lv_btn_create(wifi_modal);
    lv_obj_set_size(btn_scan, 70, 40);
    lv_obj_align_to(btn_scan, dd_ssid, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_t * lbl_scan = lv_label_create(btn_scan);
    lv_label_set_text(lbl_scan, LV_SYMBOL_REFRESH " SCAN");
    lv_obj_add_event_cb(btn_scan, scan_btn_cb, LV_EVENT_CLICKED, NULL);

    ta_pass = lv_textarea_create(wifi_modal);
    lv_textarea_set_one_line(ta_pass, true);
    lv_textarea_set_password_mode(ta_pass, true);
    lv_obj_set_width(ta_pass, 300);
    lv_obj_align(ta_pass, LV_ALIGN_TOP_LEFT, 10, 85);
    lv_textarea_set_placeholder_text(ta_pass, "Inserisci Password");
    if (strlen(bushido_pass) > 0) {
        lv_textarea_set_text(ta_pass, bushido_pass);
    }

    kb = lv_keyboard_create(wifi_modal);
    lv_obj_set_size(kb, EXAMPLE_LCD_H_RES, 170);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, ta_pass);

    lv_obj_t * btn_conn = lv_btn_create(wifi_modal);
    lv_obj_set_size(btn_conn, 120, 45);
    lv_obj_align(btn_conn, LV_ALIGN_TOP_RIGHT, -10, 35);
    lv_obj_set_style_bg_color(btn_conn, lv_color_hex(0x00AA00), 0);
    lv_obj_t * lbl_conn = lv_label_create(btn_conn);
    lv_label_set_text(lbl_conn, "CONNETTI");
    lv_obj_add_event_cb(btn_conn, connect_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_close = lv_btn_create(wifi_modal);
    lv_obj_set_size(btn_close, 120, 45);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, -10, 85);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0xAA0000), 0);
    lv_obj_t * lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "ANNULLA");
    lv_obj_add_event_cb(btn_close, close_modal_cb, LV_EVENT_CLICKED, NULL);
}

static void bushido_wifi_tile_cb(lv_event_t * e)
{
    (void)e;
    open_wifi_manager_cb(e);
}

static void bushido_audio_switch_cb(lv_event_t * e)
{
    (void)e;

    if (!ui_Switch2) return;

    bushido_audio_enabled = lv_obj_has_state(ui_Switch2, LV_STATE_CHECKED);

    if (bushido_audio_icon) {
        lv_obj_set_style_text_color(
            bushido_audio_icon,
            bushido_audio_enabled ? lv_color_hex(0xF2F2F2) : lv_color_hex(0x777777),
            LV_PART_MAIN
        );
    }

    ESP_LOGI(TAG, "Audio UI: %s", bushido_audio_enabled ? "ON" : "OFF");
}

static void bushido_bind_squareline_home(void)
{
    if (!ui_HOME || bushido_sq_home_bound) {
        return;
    }

    bushido_sq_home_bound = true;

    if (ui_topbar) {
        lv_obj_clear_flag(ui_topbar, LV_OBJ_FLAG_SCROLLABLE);
    }

    title_label = ui_Label2;
    device_label = ui_lblUser;

    if (title_label) {
        lv_label_set_text(title_label, "BUSHIDO PAD PRO");
        lv_obj_set_width(title_label, 150);
        lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(title_label, lv_color_hex(0xFF3B30), LV_PART_MAIN);
        lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 8, 0);
    }

    if (device_label) {
        lv_label_set_text_fmt(device_label, "%s", BUSHIDO_DEVICE_ID);
        lv_obj_set_width(device_label, 90);
        lv_label_set_long_mode(device_label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(device_label, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(device_label, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
        lv_obj_align(device_label, LV_ALIGN_LEFT_MID, 165, 0);
    }

    if (ui_ImgAudion) lv_obj_add_flag(ui_ImgAudion, LV_OBJ_FLAG_HIDDEN);
    if (ui_imgAllenamento) lv_obj_add_flag(ui_imgAllenamento, LV_OBJ_FLAG_HIDDEN);
    if (ui_imgDispositivo) lv_obj_add_flag(ui_imgDispositivo, LV_OBJ_FLAG_HIDDEN);
    if (ui_ImgWifi) lv_obj_add_flag(ui_ImgWifi, LV_OBJ_FLAG_HIDDEN);
    if (ui_Imgimpostazioni) lv_obj_add_flag(ui_Imgimpostazioni, LV_OBJ_FLAG_HIDDEN);

    if (!bushido_audio_icon) {
        bushido_audio_icon = lv_label_create(ui_topbar ? ui_topbar : ui_HOME);
        lv_label_set_text(bushido_audio_icon, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_color(
            bushido_audio_icon,
            bushido_audio_enabled ? lv_color_hex(0xF2F2F2) : lv_color_hex(0x777777),
            LV_PART_MAIN
        );
        lv_obj_set_style_text_font(bushido_audio_icon, &lv_font_montserrat_16, LV_PART_MAIN);

        if (ui_Switch2) {
            lv_obj_align_to(bushido_audio_icon, ui_Switch2, LV_ALIGN_OUT_LEFT_MID, -10, 0);
        } else {
            lv_obj_align(bushido_audio_icon, LV_ALIGN_TOP_RIGHT, -70, 10);
        }
    }

    if (ui_Label1) {
        lv_label_set_text(ui_Label1, LV_SYMBOL_PLAY "\nALLENAMENTO");
        lv_obj_set_width(ui_Label1, 150);
        lv_label_set_long_mode(ui_Label1, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(ui_Label1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_Label1, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_center(ui_Label1);
    }

    if (ui_DISPOSITIVO) {
        lv_label_set_text(ui_DISPOSITIVO, LV_SYMBOL_DRIVE "\nDISPOSITIVO");
        lv_obj_set_width(ui_DISPOSITIVO, 150);
        lv_label_set_long_mode(ui_DISPOSITIVO, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(ui_DISPOSITIVO, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_DISPOSITIVO, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_center(ui_DISPOSITIVO);
    }

    if (ui_WIFI) {
        lv_label_set_text(ui_WIFI, LV_SYMBOL_WIFI "\nWIFI");
        lv_obj_set_width(ui_WIFI, 150);
        lv_label_set_long_mode(ui_WIFI, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(ui_WIFI, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_WIFI, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_center(ui_WIFI);
    }

    if (ui_IMPOSTAZIONI) {
        lv_label_set_text(ui_IMPOSTAZIONI, LV_SYMBOL_SETTINGS "\nIMPOSTAZIONI");
        lv_obj_set_width(ui_IMPOSTAZIONI, 150);
        lv_label_set_long_mode(ui_IMPOSTAZIONI, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(ui_IMPOSTAZIONI, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_IMPOSTAZIONI, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_center(ui_IMPOSTAZIONI);
    }

    if (ui_btnallenamento) {
        lv_obj_add_event_cb(ui_btnallenamento, bushido_open_training_menu_page_cb, LV_EVENT_CLICKED, NULL);
    }

    if (ui_btnallenamento2) {
        lv_obj_add_event_cb(ui_btnallenamento2, bushido_wifi_tile_cb, LV_EVENT_CLICKED, NULL);
    }

    if (ui_btnallenamento1) {
        lv_obj_add_event_cb(ui_btnallenamento1, bushido_open_device_page_cb, LV_EVENT_CLICKED, NULL);
    }

    if (ui_btnallenamento3) {
        lv_obj_add_event_cb(ui_btnallenamento3, bushido_open_settings_page_cb, LV_EVENT_CLICKED, NULL);
    }

    if (ui_Switch2) {
        if (bushido_audio_enabled) {
            lv_obj_add_state(ui_Switch2, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(ui_Switch2, bushido_audio_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_mainarea) {
        lv_obj_set_y(ui_mainarea, 42);
        lv_obj_set_height(ui_mainarea, 178);
    }

    bushido_diag_panel = lv_obj_create(ui_HOME);
    lv_obj_set_size(bushido_diag_panel, EXAMPLE_LCD_H_RES - 14, 100);
    lv_obj_align(bushido_diag_panel, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_clear_flag(bushido_diag_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(bushido_diag_panel, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bushido_diag_panel, lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bushido_diag_panel, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(bushido_diag_panel, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_border_width(bushido_diag_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_left(bushido_diag_panel, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_right(bushido_diag_panel, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(bushido_diag_panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(bushido_diag_panel, 8, LV_PART_MAIN);

    lv_obj_t *diag_title = lv_label_create(bushido_diag_panel);
    lv_label_set_text(diag_title, "LIVE DATA");
    lv_obj_set_style_text_color(diag_title, lv_color_hex(0xFF3B30), LV_PART_MAIN);
    lv_obj_set_style_text_font(diag_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(diag_title, LV_ALIGN_TOP_LEFT, 0, 0);

    status_label = lv_label_create(bushido_diag_panel);
    lv_label_set_text(status_label, "READY");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF66), LV_PART_MAIN);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, 0, -2);

    hits_label = lv_label_create(bushido_diag_panel);
    lv_obj_set_width(hits_label, 210);
    lv_label_set_long_mode(hits_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(hits_label, "Hits: 0");
    lv_obj_set_style_text_color(hits_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(hits_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(hits_label, LV_ALIGN_TOP_LEFT, 0, 22);

    sensor_label = lv_label_create(bushido_diag_panel);
    lv_obj_set_width(sensor_label, 210);
    lv_label_set_long_mode(sensor_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(sensor_label, "Piezo: 0");
    lv_obj_set_style_text_color(sensor_label, lv_color_hex(0xCFCFCF), LV_PART_MAIN);
    lv_obj_set_style_text_font(sensor_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(sensor_label, LV_ALIGN_TOP_RIGHT, 0, 22);

    force_label = lv_label_create(bushido_diag_panel);
    lv_obj_set_width(force_label, 210);
    lv_label_set_long_mode(force_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(force_label, "LC: 0N | SPD:0.0");
    lv_obj_set_style_text_color(force_label, lv_color_hex(0xFFFF66), LV_PART_MAIN);
    lv_obj_set_style_text_font(force_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(force_label, LV_ALIGN_TOP_LEFT, 0, 42);

    loadcell_label = lv_label_create(bushido_diag_panel);
    lv_obj_set_width(loadcell_label, 210);
    lv_label_set_long_mode(loadcell_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(loadcell_label, "Raw: 0");
    lv_obj_set_style_text_color(loadcell_label, lv_color_hex(0x66CCFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(loadcell_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(loadcell_label, LV_ALIGN_TOP_RIGHT, 0, 42);

    wifi_label = lv_label_create(bushido_diag_panel);
    lv_obj_set_width(wifi_label, 210);
    lv_label_set_long_mode(wifi_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(wifi_label, wifi_status_text);
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xAFAFAF), LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(wifi_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    wrist_label = lv_label_create(bushido_diag_panel);
    lv_obj_set_width(wrist_label, 210);
    lv_label_set_long_mode(wrist_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(wrist_label, "W:0/4 in attesa");
    lv_obj_set_style_text_color(wrist_label, lv_color_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_set_style_text_font(wrist_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(wrist_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

// ======================================================
// HARDWARE / DISPLAY
// ======================================================
void play_embedded_wav()
{
    if (!bushido_audio_enabled || bushido_audio_volume <= 0) {
        return;
    }

    const uint8_t *data = boot_sound_wav_start;
    size_t size = boot_sound_wav_end - boot_sound_wav_start;

    if (size <= 44) {
        ESP_LOGE(TAG, "boot_sound.wav non valido");
        return;
    }

    const uint8_t *audio = data + 44;
    size_t audio_size = size - 44;

    extern esp_codec_dev_handle_t output_dev;

    esp_codec_dev_set_out_vol(output_dev, (float)bushido_audio_volume);

    int err = esp_codec_dev_write(output_dev, (void *)audio, (int)audio_size);
    if (err != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Errore audio: %d", err);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    esp_codec_dev_set_out_vol(output_dev, 0.0f);
}

void i2c_bus_init(void)
{
    i2c_master_bus_config_t i2c_mst_config = {};
    i2c_mst_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port = (i2c_port_num_t)I2C_PORT_NUM;
    i2c_mst_config.scl_io_num = EXAMPLE_PIN_I2C_SCL;
    i2c_mst_config.sda_io_num = EXAMPLE_PIN_I2C_SDA;
    i2c_mst_config.glitch_ignore_cnt = 7;
    i2c_mst_config.flags.enable_internal_pullup = 1;

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));
}

void io_expander_init(void)
{
    ESP_ERROR_CHECK(
        esp_io_expander_new_i2c_tca9554(
            i2c_bus_handle,
            ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000,
            &expander_handle
        )
    );

    ESP_ERROR_CHECK(esp_io_expander_set_dir(expander_handle, IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_OUTPUT));
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 1));
    vTaskDelay(pdMS_TO_TICKS(100));
}

void lv_port_init(void)
{
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    lvgl_port_display_cfg_t display_cfg = {};
    display_cfg.io_handle = io_handle;
    display_cfg.panel_handle = panel_handle;
    display_cfg.control_handle = NULL;
    display_cfg.buffer_size = LCD_BUFFER_SIZE;
    display_cfg.double_buffer = true;
    display_cfg.trans_size = 0;
    display_cfg.hres = EXAMPLE_LCD_H_RES;
    display_cfg.vres = EXAMPLE_LCD_V_RES;
    display_cfg.monochrome = false;

    display_cfg.rotation.swap_xy = 0;
    display_cfg.rotation.mirror_x = 1;
    display_cfg.rotation.mirror_y = 0;

    display_cfg.flags.buff_dma = 0;
    display_cfg.flags.buff_spiram = 1;
    display_cfg.flags.sw_rotate = 1;
    display_cfg.flags.full_refresh = 0;
    display_cfg.flags.direct_mode = 0;

#if EXAMPLE_DISPLAY_ROTATION == 90
    display_cfg.rotation.swap_xy = 1;
    display_cfg.rotation.mirror_x = 1;
    display_cfg.rotation.mirror_y = 1;
#elif EXAMPLE_DISPLAY_ROTATION == 180
    display_cfg.rotation.swap_xy = 0;
    display_cfg.rotation.mirror_x = 0;
    display_cfg.rotation.mirror_y = 1;
#elif EXAMPLE_DISPLAY_ROTATION == 270
    display_cfg.rotation.swap_xy = 1;
    display_cfg.rotation.mirror_x = 0;
    display_cfg.rotation.mirror_y = 0;
#endif

    lvgl_disp = lvgl_port_add_disp(&display_cfg);

    lvgl_port_touch_cfg_t touch_cfg = {};
    touch_cfg.disp = lvgl_disp;
    touch_cfg.handle = touch_handle;

    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
}

void bushido_show_splash(void)
{
    if (!bushido_sq_ui_initialized) {
        ui_init();
        bushido_sq_ui_initialized = true;
    }

    if (ui_SPLASH) {
        lv_obj_set_style_bg_color(ui_SPLASH, lv_color_hex(0x050505), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui_SPLASH, LV_OPA_COVER, LV_PART_MAIN);
    }

    if (ui_logoteclab) {
        lv_obj_add_flag(ui_logoteclab, LV_OBJ_FLAG_HIDDEN);
    }

    if (ui_Label3) {
        lv_label_set_text(ui_Label3, "BUSHIDO TECLAB");
        lv_obj_set_style_text_color(ui_Label3, lv_color_hex(0xF2F2F2), LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_Label3, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_align(ui_Label3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(ui_Label3, LV_ALIGN_CENTER, 0, -24);
    }

    if (ui_Label4) {
        char fw_buf[96];
        lv_snprintf(fw_buf, sizeof(fw_buf), "Smart Combat Training\nFW %s", BUSHIDO_FIRMWARE_VERSION);
        lv_label_set_text(ui_Label4, fw_buf);
        lv_obj_set_style_text_color(ui_Label4, lv_color_hex(0xA8A8A8), LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_Label4, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_align(ui_Label4, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(ui_Label4, LV_ALIGN_CENTER, 0, 18);
    }

    if (ui_Bar2) {
        lv_obj_set_size(ui_Bar2, 170, 8);
        lv_obj_align(ui_Bar2, LV_ALIGN_BOTTOM_MID, 0, -24);
        lv_bar_set_value(ui_Bar2, 100, LV_ANIM_OFF);
    }

    if (ui_SPLASH) {
        lv_disp_load_scr(ui_SPLASH);
    }
}

void bushido_ui_init(void)
{
    if (!bushido_sq_ui_initialized) {
        ui_init();
        bushido_sq_ui_initialized = true;
    }

    if (ui_HOME) {
        lv_disp_load_scr(ui_HOME);
    }

    bushido_bind_squareline_home();

    bushido_set_wifi_text(wifi_status_text);

    if (wrist_label) {
        lv_label_set_text(wrist_label, "W:0/4 in attesa");
    }

    bushido_refresh_settings_page_ui();

    bushido_ui_show_ready();

    if (force_label) lv_label_set_text(force_label, "LC: 0N | SPD:0.0");
    if (sensor_label) lv_label_set_text(sensor_label, "Piezo: 0");
    if (loadcell_label) lv_label_set_text(loadcell_label, "Raw: 0");
}

void bushido_set_wifi_text(const char *text)
{
    if (wifi_label) lv_label_set_text(wifi_label, text);
}

void bushido_set_wrist_text(const char *text)
{
    if (wrist_label) lv_label_set_text(wrist_label, text);
}

void bushido_ui_show_ready(void)
{
    if (status_label) {
        lv_label_set_text(status_label, "READY");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF66), LV_PART_MAIN);
    }

    if (hits_label) {
        lv_label_set_text_fmt(hits_label, "Hits: %d", bushido_total_hits);
    }
}

void bushido_ui_show_hit(int force_value, float speed_value, int sensor_value)
{
    if (status_label) {
        lv_label_set_text(status_label, "COLPO!");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF3333), LV_PART_MAIN);
    }

    if (hits_label) {
        lv_label_set_text_fmt(hits_label, "Hits: %d", bushido_total_hits);
    }

    if (force_label) {
        char buf[64];
        lv_snprintf(buf, sizeof(buf), "LC:%dN | SPD:%.1f", force_value, speed_value);
        lv_label_set_text(force_label, buf);
    }

    if (sensor_label) {
        lv_label_set_text_fmt(sensor_label, "Piezo: %d", sensor_value);
    }

    if (loadcell_label) {
        lv_label_set_text_fmt(loadcell_label, "Raw: %ld", bushido_last_loadcell_raw);
    }
}

// ======================================================
// ADC / HX711
// ======================================================
void bushido_sensor_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {};
    init_config.unit_id = BUSHIDO_SENSOR_ADC_UNIT;
    init_config.ulp_mode = ADC_ULP_MODE_DISABLE;

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t config = {};
    config.atten = ADC_ATTEN_DB_12;
    config.bitwidth = ADC_BITWIDTH_DEFAULT;

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, BUSHIDO_SENSOR_ADC_CHANNEL, &config));
}

void bushido_hx711_init(void)
{
    gpio_config_t clk_config = {};
    clk_config.intr_type = GPIO_INTR_DISABLE;
    clk_config.mode = GPIO_MODE_OUTPUT;
    clk_config.pin_bit_mask = (1ULL << BUSHIDO_HX711_CLK_GPIO);
    clk_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    clk_config.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&clk_config));

    gpio_config_t data_config = {};
    data_config.intr_type = GPIO_INTR_DISABLE;
    data_config.mode = GPIO_MODE_INPUT;
    data_config.pin_bit_mask = (1ULL << BUSHIDO_HX711_DATA_GPIO);
    data_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    data_config.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&data_config));

    gpio_set_level(BUSHIDO_HX711_CLK_GPIO, 0);
}

bool bushido_hx711_is_ready(void)
{
    return gpio_get_level(BUSHIDO_HX711_DATA_GPIO) == 0;
}

long bushido_hx711_read_raw(void)
{
    long count = 0;
    if (!bushido_hx711_is_ready()) return bushido_last_loadcell_raw;

    for (int i = 0; i < 24; i++) {
        gpio_set_level(BUSHIDO_HX711_CLK_GPIO, 1);
        esp_rom_delay_us(1);
        count = count << 1;
        gpio_set_level(BUSHIDO_HX711_CLK_GPIO, 0);
        esp_rom_delay_us(1);
        if (gpio_get_level(BUSHIDO_HX711_DATA_GPIO)) count++;
    }

    gpio_set_level(BUSHIDO_HX711_CLK_GPIO, 1);
    esp_rom_delay_us(1);
    gpio_set_level(BUSHIDO_HX711_CLK_GPIO, 0);
    esp_rom_delay_us(1);

    if (count & 0x800000) count |= ~0xFFFFFF;
    return count;
}

// ======================================================
// BACKEND
// ======================================================
void send_hit_to_backend(int force_value, float speed_value, int angle_deg, const char* strike_type, const char *zone_value)
{
    char payload[512];

    const char *safe_type = (strike_type && strlen(strike_type) > 0) ? strike_type : "unknown";
    const char *safe_zone = (zone_value && strlen(zone_value) > 0) ? zone_value : "center";

    if (speed_value > 0.0f) {
        snprintf(
            payload,
            sizeof(payload),
            "{\"deviceId\":\"%s\",\"force_n\":%d,\"zone\":\"%s\",\"speed_ms\":%.2f,\"angle_deg\":%d,\"strike_type\":\"%s\",\"sourceModules\":[\"pad\",\"wrist\"]}",
            BUSHIDO_DEVICE_ID,
            force_value,
            safe_zone,
            speed_value,
            angle_deg,
            safe_type
        );
    } else {
        snprintf(
            payload,
            sizeof(payload),
            "{\"deviceId\":\"%s\",\"force_n\":%d,\"zone\":\"%s\",\"sourceModules\":[\"pad\"]}",
            BUSHIDO_DEVICE_ID,
            force_value,
            safe_zone
        );
    }

    esp_http_client_config_t config = {};
    config.url = BUSHIDO_BACKEND_HIT_URL;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 5000;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "HTTP client init failed");
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, strlen(payload));

    ESP_LOGI(TAG, "Invio Payload: %s", payload);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Status: %d", status);
        if (lvgl_port_lock(0)) {
            bushido_set_wifi_text("WiFi: hit inviato");
            lvgl_port_unlock();
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
        if (lvgl_port_lock(0)) {
            bushido_set_wifi_text("WiFi: errore invio");
            lvgl_port_unlock();
        }
    }

    esp_http_client_cleanup(client);
}

static esp_err_t bushido_register_wrist_peer(const uint8_t *mac)
{
    if (!mac) {
        return ESP_ERR_INVALID_ARG;
    }

    bushido_refresh_radio_channel();

    if (esp_now_is_peer_exist(mac)) {
        esp_now_del_peer(mac);
    }

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = g_radio_channel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err == ESP_ERR_ESPNOW_EXIST) {
        return ESP_OK;
    }
    return err;
}

static void bushido_send_pair_ack_to_wrist_slot(int slot_index)
{
    if (slot_index < 0 || slot_index >= BUSHIDO_MAX_WRISTS) return;
    if (!g_wrist_slots[slot_index].occupied) return;

    esp_err_t peer_err = bushido_register_wrist_peer(g_wrist_slots[slot_index].mac);
    if (peer_err != ESP_OK) {
        ESP_LOGE(TAG, "Pair ACK peer register failed: %s", esp_err_to_name(peer_err));
        return;
    }

    bushido_refresh_radio_channel();

    pad_pair_ack_t ack = {};
    strncpy(ack.type, "pair_ack", sizeof(ack.type) - 1);
    strncpy(ack.device_id, BUSHIDO_DEVICE_ID, sizeof(ack.device_id) - 1);
    ack.channel = g_radio_channel;
    ack.slot_index = (uint8_t)slot_index;

    esp_err_t err = esp_now_send(g_wrist_slots[slot_index].mac, (uint8_t *)&ack, sizeof(ack));
    if (err == ESP_OK) {
        g_wrist_slots[slot_index].pair_ack_sent = true;
        ESP_LOGI(TAG, "Pair ACK inviato a %s | slot=%d | ch=%d",
                 g_wrist_slots[slot_index].device_id,
                 slot_index,
                 g_radio_channel);
    } else {
        ESP_LOGE(TAG, "Errore invio pair_ack: %s", esp_err_to_name(err));
    }
}

static void bushido_send_hit_confirm_to_wrist_slot(int slot_index, int force_value, const char *zone_value)
{
    if (slot_index < 0 || slot_index >= BUSHIDO_MAX_WRISTS) {
        ESP_LOGW(TAG, "Slot wrist non valido per hit_confirm");
        return;
    }

    if (!g_wrist_slots[slot_index].occupied) {
        ESP_LOGW(TAG, "Slot wrist vuoto per hit_confirm");
        return;
    }

    esp_err_t peer_err = bushido_register_wrist_peer(g_wrist_slots[slot_index].mac);
    if (peer_err != ESP_OK) {
        ESP_LOGE(TAG, "Peer wrist non aggiornabile: %s", esp_err_to_name(peer_err));
        return;
    }

    pad_confirm_t confirm = {};
    strncpy(confirm.type, "hit_confirm", sizeof(confirm.type) - 1);
    strncpy(confirm.zone, zone_value ? zone_value : "center", sizeof(confirm.zone) - 1);
    confirm.force_value = force_value;

    esp_err_t err = esp_now_send(g_wrist_slots[slot_index].mac, (uint8_t *)&confirm, sizeof(confirm));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Errore invio hit_confirm al wrist[%d]: %s", slot_index, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "hit_confirm inviato al wrist[%d] %s | force=%d",
                 slot_index,
                 g_wrist_slots[slot_index].device_id,
                 force_value);
    }
}

void bushido_http_task(void *pvParameters)
{
    bushido_hit_event_t event;
    while (true)
    {
        if (xQueueReceive(g_hit_queue, &event, portMAX_DELAY)) {
            send_hit_to_backend(event.force_value, event.speed_ms, event.angle_deg, event.strike_type, event.zone);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

// ======================================================
// SENSOR TASK PAD
// ======================================================
void bushido_sensor_task(void *pvParameters)
{
    int piezo_value = 0;
    long loadcell_raw = 0;

    static bool pending_fusion = false;
    static unsigned long pending_fusion_ms = 0;
    static int pending_force_value = 0;
    static int pending_sensor_value = 0;

    while (true)
    {
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, BUSHIDO_SENSOR_ADC_CHANNEL, &piezo_value));

        if (bushido_hx711_is_ready()) {
            loadcell_raw = bushido_hx711_read_raw();
            bushido_last_loadcell_raw = loadcell_raw - bushido_loadcell_offset;
        }

        long lc_abs = bushido_abs_long(bushido_last_loadcell_raw);
        unsigned long now_ms = (unsigned long)esp_log_timestamp();

        if ((now_ms - bushido_last_debug_ms) > BUSHIDO_DEBUG_INTERVAL_MS) {
            bushido_last_debug_ms = now_ms;
            if (lvgl_port_lock(0)) {
                if (sensor_label) lv_label_set_text_fmt(sensor_label, "Piezo: %d", piezo_value);
                if (loadcell_label) lv_label_set_text_fmt(loadcell_label, "LoadCell raw: %ld", bushido_last_loadcell_raw);

                if (bushido_device_page_get_root()) {
                    bushido_refresh_device_page_ui(piezo_value);
                }

                lvgl_port_unlock();
            }
        }

        if (lc_abs > BUSHIDO_LOADCELL_HIT_THRESHOLD &&
            !bushido_hit_active &&
            (now_ms - bushido_last_hit_ms > BUSHIDO_HIT_COOLDOWN_MS)) {

            int force_value = bushido_loadcell_raw_to_force_n(lc_abs);
            bool has_wrists = bushido_count_paired_wrists() > 0;
            bool consumed_by_training = false;

            if (bushido_training_running && !bushido_training_paused) {
                consumed_by_training = bushido_training_handle_pad_hit(now_ms);
            }

            bushido_total_hits++;
            bushido_hit_active = true;
            bushido_last_hit_ms = now_ms;

            if (consumed_by_training) {
                if (lvgl_port_lock(0)) {
                    bushido_refresh_training_page_ui();
                    lvgl_port_unlock();
                }
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }

            if (lvgl_port_lock(0)) {
                bushido_ui_show_hit(force_value, 0.0f, piezo_value);
                lvgl_port_unlock();
            }

            if (!has_wrists) {
                if (g_hit_queue != NULL) {
                    bushido_hit_event_t event = {};
                    event.force_value = force_value;
                    event.speed_ms = 0.0f;
                    event.angle_deg = 0;

                    strncpy(event.strike_type, "unknown", sizeof(event.strike_type) - 1);
                    event.strike_type[sizeof(event.strike_type) - 1] = '\0';

                    strncpy(event.zone, "center", sizeof(event.zone) - 1);
                    event.zone[sizeof(event.zone) - 1] = '\0';

                    strncpy(event.wrist_device_id, "-", sizeof(event.wrist_device_id) - 1);
                    event.wrist_device_id[sizeof(event.wrist_device_id) - 1] = '\0';

                    if (xQueueSend(g_hit_queue, &event, 0) != pdPASS) {
                        ESP_LOGW(TAG, "Coda hit piena, evento scartato");
                    }
                }

                ESP_LOGW(TAG, "TRIGGER LOADCELL IMMEDIATO -> force=%d | lc_raw=%ld | piezo=%d",
                         force_value, bushido_last_loadcell_raw, piezo_value);
            } else {
                pending_fusion = true;
                pending_fusion_ms = now_ms;
                pending_force_value = force_value;
                pending_sensor_value = piezo_value;

                ESP_LOGW(TAG, "TRIGGER LOADCELL FUSION -> force=%d | lc_raw=%ld | piezo=%d",
                         force_value, bushido_last_loadcell_raw, piezo_value);
            }
        }

        if (pending_fusion && (now_ms - pending_fusion_ms >= BUSHIDO_FUSION_DELAY_MS)) {
            pending_fusion = false;

            float fused_speed = 0.0f;
            int fused_angle = 0;
            char fused_type[16] = "unknown";
            char fused_device_id[16] = "-";
            int best_slot = bushido_select_best_wrist_slot(pending_fusion_ms);
            bool fusion_ok = false;

            if (best_slot >= 0) {
                fused_speed = g_wrist_slots[best_slot].last_speed_raw / 100.0f;
                fused_angle = g_wrist_slots[best_slot].last_angle;

                strncpy(fused_type, g_wrist_slots[best_slot].last_type, sizeof(fused_type) - 1);
                fused_type[sizeof(fused_type) - 1] = '\0';

                strncpy(fused_device_id, g_wrist_slots[best_slot].device_id, sizeof(fused_device_id) - 1);
                fused_device_id[sizeof(fused_device_id) - 1] = '\0';

                fusion_ok = true;

                ESP_LOGW(TAG, "FUSION OK -> force=%d speed=%.2f angle=%d type=%s wrist=%s",
                         pending_force_value, fused_speed, fused_angle, fused_type, fused_device_id);
            }

            if (!fusion_ok) {
                ESP_LOGI(TAG, "SOLO LOADCELL -> nessun dato wrist valido");
            }

            if (g_hit_queue != NULL) {
                bushido_hit_event_t event = {};
                event.force_value = pending_force_value;
                event.speed_ms = fused_speed;
                event.angle_deg = fused_angle;
                strncpy(event.strike_type, fused_type, sizeof(event.strike_type) - 1);
                event.strike_type[sizeof(event.strike_type) - 1] = '\0';
                strncpy(event.zone, "center", sizeof(event.zone) - 1);
                event.zone[sizeof(event.zone) - 1] = '\0';
                strncpy(event.wrist_device_id, fused_device_id, sizeof(event.wrist_device_id) - 1);
                event.wrist_device_id[sizeof(event.wrist_device_id) - 1] = '\0';

                if (xQueueSend(g_hit_queue, &event, 0) != pdPASS) {
                    ESP_LOGW(TAG, "Coda hit piena, evento scartato");
                }
            }

            if (fusion_ok) {
                bushido_send_hit_confirm_to_wrist_slot(best_slot, pending_force_value, "center");
            }

            if (lvgl_port_lock(0)) {
                bushido_ui_show_hit(pending_force_value, fused_speed, pending_sensor_value);
                lvgl_port_unlock();
            }
        }

        if (lc_abs < BUSHIDO_LOADCELL_RELEASE_TH) {
            if (bushido_hit_active && !pending_fusion) {
                if (lvgl_port_lock(0)) {
                    bushido_ui_show_ready();
                    lvgl_port_unlock();
                }
            }
            bushido_hit_active = false;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ======================================================
// ESP-NOW CALLBACK
// ======================================================
static void bushido_espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len)
{
    if (!esp_now_info || !data) {
        return;
    }

    if (data_len == (int)sizeof(wrist_pair_req_t)) {
        wrist_pair_req_t req = {};
        memcpy(&req, data, sizeof(req));

        if (strncmp(req.type, "pair_request", 12) == 0) {
            int slot = bushido_allocate_or_get_wrist_slot(esp_now_info->src_addr, req.device_id);
            if (slot >= 0) {
                esp_err_t add_err = bushido_register_wrist_peer(esp_now_info->src_addr);
                if (add_err == ESP_OK) {
                    char macbuf[24];
                    bushido_mac_to_string(esp_now_info->src_addr, macbuf, sizeof(macbuf));
                    ESP_LOGW(TAG, "PAIR REQUEST <- %s | %s | slot=%d",
                             req.device_id, macbuf, slot);

                    bushido_send_pair_ack_to_wrist_slot(slot);
                    bushido_update_wrist_ui_summary("-", g_wrist_slots[slot].device_id, 0.0f);
                } else {
                    ESP_LOGE(TAG, "Errore register peer da pair_request: %s", esp_err_to_name(add_err));
                }
            }
        }

        return;
    }

    if (data_len == (int)sizeof(wrist_data_t)) {
        wrist_data_t incoming_wrist_data = {};
        memcpy(&incoming_wrist_data, data, sizeof(incoming_wrist_data));

        int slot = bushido_allocate_or_get_wrist_slot(esp_now_info->src_addr, incoming_wrist_data.device_id);
        if (slot < 0) {
            ESP_LOGW(TAG, "Pacchetto wrist scartato: nessuno slot libero");
            return;
        }

        esp_err_t add_err = bushido_register_wrist_peer(esp_now_info->src_addr);
        if (add_err != ESP_OK) {
            ESP_LOGE(TAG, "Errore add/update peer wrist: %s", esp_err_to_name(add_err));
            return;
        }

        float ax = incoming_wrist_data.acc_x;
        float ay = incoming_wrist_data.acc_y;
        float az = incoming_wrist_data.acc_z;
        float speed = sqrtf((ax * ax) + (ay * ay) + (az * az));

        unsigned long now_ms = (unsigned long)esp_log_timestamp();

        g_wrist_slots[slot].last_speed_raw = speed;
        g_wrist_slots[slot].last_speed_ms = now_ms;
        g_wrist_slots[slot].last_angle = incoming_wrist_data.angle_deg;

        strncpy(g_wrist_slots[slot].last_type,
                incoming_wrist_data.strike_type,
                sizeof(g_wrist_slots[slot].last_type) - 1);
        g_wrist_slots[slot].last_type[sizeof(g_wrist_slots[slot].last_type) - 1] = '\0';

        ESP_LOGI(TAG, "WRIST RX -> slot=%d dev=%s speed=%.2f angle=%d type=%s",
                 slot,
                 g_wrist_slots[slot].device_id,
                 g_wrist_slots[slot].last_speed_raw,
                 g_wrist_slots[slot].last_angle,
                 g_wrist_slots[slot].last_type);

        bushido_update_wrist_ui_summary(
            g_wrist_slots[slot].last_type,
            g_wrist_slots[slot].device_id,
            g_wrist_slots[slot].last_speed_raw
        );

        return;
    }

    ESP_LOGW(TAG, "Pacchetto ESP-NOW errato: len=%d", data_len);
}

void bushido_espnow_init(void)
{
    ESP_LOGI(TAG, "Inizializzazione ESP-NOW...");
    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Errore esp_now_init: %s", esp_err_to_name(err));
        return;
    }

    err = esp_now_register_recv_cb(bushido_espnow_recv_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Errore esp_now_register_recv_cb: %s", esp_err_to_name(err));
        return;
    }

    bushido_refresh_radio_channel();
    ESP_LOGI(TAG, "ESP-NOW Inizializzato. In ascolto wrist su ch=%d", g_radio_channel);
}

// ======================================================
// APP MAIN
// ======================================================
extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    memset(g_wrist_slots, 0, sizeof(g_wrist_slots));

    load_wifi_credentials();

    i2c_bus_init();

    esp_axp2101_port_init(i2c_bus_handle);
    vTaskDelay(pdMS_TO_TICKS(100));

    io_expander_init();

    esp_3inch5_display_port_init(&io_handle, &panel_handle, LCD_BUFFER_SIZE);
    esp_3inch5_touch_port_init(
        &touch_handle,
        i2c_bus_handle,
        EXAMPLE_LCD_H_RES,
        EXAMPLE_LCD_V_RES,
        EXAMPLE_DISPLAY_ROTATION
    );

    esp_es8311_port_init(i2c_bus_handle);
    vTaskDelay(pdMS_TO_TICKS(200));

    esp_qmi8658_port_init(i2c_bus_handle);
    esp_pcf85063_port_init(i2c_bus_handle);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_channel(BUSHIDO_ESPNOW_FALLBACK_CHANNEL, WIFI_SECOND_CHAN_NONE));
    g_radio_channel = BUSHIDO_ESPNOW_FALLBACK_CHANNEL;

    if (strlen(bushido_ssid) > 0) {
        wifi_config_t wifi_config = {};

        strncpy((char *)wifi_config.sta.ssid, bushido_ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char *)wifi_config.sta.password, bushido_pass, sizeof(wifi_config.sta.password) - 1);

        if (strlen(bushido_pass) == 0) {
            wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        } else {
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
            wifi_config.sta.pmf_cfg.capable = true;
            wifi_config.sta.pmf_cfg.required = false;
        }

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

        esp_err_t err_conn = esp_wifi_connect();
        if (err_conn != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err_conn));
            wifi_connecting = false;
            wifi_connected = false;
            snprintf(wifi_status_text, sizeof(wifi_status_text), "WiFi: ERRORE CONNESSIONE");
        } else {
            wifi_connecting = true;
            wifi_connected = false;
            snprintf(wifi_status_text, sizeof(wifi_status_text), "WiFi: Connessione a %s...", bushido_ssid);
        }
    } else {
        snprintf(wifi_status_text, sizeof(wifi_status_text), "WiFi: RETE NON CONFIGURATA");
    }

    bushido_refresh_radio_channel();

    ESP_LOGI(TAG, "DIM wrist_data_t = %d", (int)sizeof(wrist_data_t));
    ESP_LOGI(TAG, "DIM pad_confirm_t = %d", (int)sizeof(pad_confirm_t));
    ESP_LOGI(TAG, "DIM wrist_pair_req_t = %d", (int)sizeof(wrist_pair_req_t));
    ESP_LOGI(TAG, "DIM pad_pair_ack_t = %d", (int)sizeof(pad_pair_ack_t));
    ESP_LOGI(TAG, "Radio channel iniziale = %d", g_radio_channel);

    bushido_espnow_init();

    esp_3inch5_brightness_port_init();
    esp_3inch5_brightness_port_set(bushido_display_brightness);

    vTaskDelay(pdMS_TO_TICKS(150));
    play_embedded_wav();

    lv_port_init();
    bushido_sensor_init();
    bushido_hx711_init();

    ESP_LOGI(TAG, "Calibrazione load cell...");

    long sum = 0;
    int samples = 20;
    int valid_samples = 0;

    while (valid_samples < samples) {
        if (bushido_hx711_is_ready()) {
            long val = bushido_hx711_read_raw();
            sum += val;
            valid_samples++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    bushido_loadcell_offset = sum / samples;
    ESP_LOGI(TAG, "LoadCell OFFSET: %ld", bushido_loadcell_offset);

    if (lvgl_port_lock(0))
    {
        bushido_show_splash();
        lvgl_port_unlock();
    }

    vTaskDelay(pdMS_TO_TICKS(2000));

    if (lvgl_port_lock(0))
    {
        bushido_ui_init();
        lvgl_port_unlock();
    }

    g_hit_queue = xQueueCreate(10, sizeof(bushido_hit_event_t));
    if (g_hit_queue == NULL) {
        ESP_LOGE(TAG, "Errore creazione coda hit");
    }

    xTaskCreate(bushido_sensor_task, "bushido_sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(bushido_http_task, "bushido_http_task", 6144, NULL, 4, NULL);
    xTaskCreate(bushido_training_task, "bushido_training_task", 4096, NULL, 3, &bushido_training_task_handle);
    ESP_LOGI(TAG, "Bushido Power Pad avviato");

    static int connect_attempts = 0;
    static uint8_t last_logged_channel = 0;

    while (1) {
        wifi_ap_record_t ap_info;

        bushido_refresh_radio_channel();
        if (g_radio_channel != last_logged_channel) {
            last_logged_channel = g_radio_channel;
            ESP_LOGI(TAG, "Radio channel aggiornato: %d", g_radio_channel);
        }

        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            if (!wifi_connected) {
                wifi_connected = true;
                wifi_connecting = false;
                connect_attempts = 0;
                snprintf(wifi_status_text, sizeof(wifi_status_text), "WiFi: CONNESSO (%s) ch=%d", bushido_ssid, ap_info.primary);
                update_wifi_ui_status(wifi_status_text, lv_color_hex(0x00FF00));
            }
        } else {
            if (strlen(bushido_ssid) == 0) {
                if (strcmp(wifi_status_text, "WiFi: RETE NON CONFIGURATA") != 0) {
                    wifi_connected = false;
                    wifi_connecting = false;
                    connect_attempts = 0;
                    snprintf(wifi_status_text, sizeof(wifi_status_text), "WiFi: RETE NON CONFIGURATA");
                    update_wifi_ui_status(wifi_status_text, lv_color_hex(0xAAAAAA));
                }
            } else if (wifi_connecting) {
                connect_attempts++;
                if (connect_attempts > 5) {
                    wifi_connecting = false;
                    wifi_connected = false;
                    connect_attempts = 0;
                    snprintf(wifi_status_text, sizeof(wifi_status_text), "WiFi: DISCONNESSO");
                    update_wifi_ui_status(wifi_status_text, lv_color_hex(0xFF0000));
                }
            } else if (wifi_connected || strcmp(wifi_status_text, "WiFi: DISCONNESSO") != 0) {
                wifi_connected = false;
                snprintf(wifi_status_text, sizeof(wifi_status_text), "WiFi: DISCONNESSO");
                update_wifi_ui_status(wifi_status_text, lv_color_hex(0xFF0000));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}