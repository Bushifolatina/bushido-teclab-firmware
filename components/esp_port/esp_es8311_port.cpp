#include "esp_idf_version.h"

#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "soc/soc_caps.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

#include "driver/i2c_master.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <math.h>
#include "esp_heap_caps.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define USE_IDF_I2C_MASTER

#define I2S_MCK_PIN 12
#define I2S_BCK_PIN 13
#define I2S_LRCK_PIN 15
#define I2S_DOUT_PIN 16
#define I2S_DIN_PIN 14

i2s_chan_handle_t tx_handle;
i2s_chan_handle_t rx_handle;
esp_codec_dev_handle_t output_dev;
esp_codec_dev_handle_t input_dev;

static esp_err_t es8311_i2s_init(void)
{
    esp_err_t esp_err;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_std_config_t std_cfg = {};

    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t)16, I2S_SLOT_MODE_STEREO);

    std_cfg.gpio_cfg.mclk = (gpio_num_t)I2S_MCK_PIN;
    std_cfg.gpio_cfg.bclk = (gpio_num_t)I2S_BCK_PIN;
    std_cfg.gpio_cfg.ws = (gpio_num_t)I2S_LRCK_PIN;
    std_cfg.gpio_cfg.dout = (gpio_num_t)I2S_DOUT_PIN;
    std_cfg.gpio_cfg.din = (gpio_num_t)I2S_DIN_PIN;

    esp_err = i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
    if (esp_err != ESP_OK) {
        return esp_err;
    }

    esp_err = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (esp_err != ESP_OK) {
        return esp_err;
    }

    esp_err = i2s_channel_init_std_mode(rx_handle, &std_cfg);
    if (esp_err != ESP_OK) {
        return esp_err;
    }

    i2s_channel_enable(tx_handle);
    i2s_channel_enable(rx_handle);

    return ESP_OK;
}

void esp_es8311_port_init(i2c_master_bus_handle_t bus_handle)
{
    es8311_i2s_init();

    audio_codec_i2s_cfg_t i2s_cfg = {
        .rx_handle = rx_handle,
        .tx_handle = tx_handle,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    static audio_codec_i2c_cfg_t i2c_cfg = {};
    i2c_cfg.addr = ES8311_CODEC_DEFAULT_ADDR;
    i2c_cfg.bus_handle = bus_handle;

    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8311_cfg.ctrl_if = ctrl_if;
    es8311_cfg.gpio_if = gpio_if;
    es8311_cfg.pa_pin = GPIO_NUM_NC;
    es8311_cfg.use_mclk = true;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;

    const audio_codec_if_t *codec_if = es8311_codec_new(&es8311_cfg);

    static esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    output_dev = esp_codec_dev_new(&dev_cfg);
    assert(output_dev != NULL);

    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    input_dev = esp_codec_dev_new(&dev_cfg);
    assert(input_dev != NULL);

    esp_codec_set_disable_when_closed(output_dev, false);
    esp_codec_set_disable_when_closed(input_dev, false);

    esp_codec_dev_sample_info_t fs = {};
    fs.sample_rate = 48000;
    fs.channel = 1;
    fs.bits_per_sample = 16;
    fs.channel_mask = 0;
    fs.mclk_multiple = 0;

    esp_codec_dev_open(output_dev, &fs);
    esp_codec_dev_open(input_dev, &fs);
}

void esp_es8311_test(void)
{
    int err = 0;
    const int limit_size = 2 * 48000 * 1 * (16 >> 3);

    uint8_t *data = (uint8_t *)heap_caps_malloc(limit_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!data) {
        printf("Audio test alloc failed\n");
        return;
    }

    esp_codec_dev_set_in_gain(input_dev, 40.0);
    err = esp_codec_dev_read(input_dev, data, limit_size);
    esp_codec_dev_set_in_gain(input_dev, 0.0);

    if (err == ESP_CODEC_DEV_OK) {
        printf("Read %d bytes\n", limit_size);
    } else {
        printf("Read error %d\n", err);
    }

    esp_codec_dev_set_out_vol(output_dev, 70.0);
    err = esp_codec_dev_write(output_dev, data, limit_size);
    esp_codec_dev_set_out_vol(output_dev, 0.0);

    if (err == ESP_CODEC_DEV_OK) {
        printf("Write %d bytes\n", limit_size);
    } else {
        printf("Write error %d\n", err);
    }

    heap_caps_free(data);
}

void esp_es8311_play_boot_sound(void)
{
    const int sample_rate = 48000;
    const int total_ms = 400;
    const int total_samples = (sample_rate * total_ms) / 1000;
    const float amp = 12000.0f;

    int16_t *buffer = (int16_t *)heap_caps_malloc(
        total_samples * 2 * sizeof(int16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    if (!buffer) {
        printf("Audio alloc failed\n");
        return;
    }

    for (int i = 0; i < total_samples; i++) {
        float t = (float)i / (float)sample_rate;

        float f1 = 500.0f;
        float f2 = 800.0f;
        float f3 = 1100.0f;

        float env = 1.0f;
        if (t < 0.05f) {
            env = t / 0.05f;
        } else if (t > 0.3f) {
            env = 1.0f - ((t - 0.3f) / 0.1f);
            if (env < 0.0f) {
                env = 0.0f;
            }
        }

        float s =
            0.6f * sinf(2.0f * (float)M_PI * f1 * t) +
            0.3f * sinf(2.0f * (float)M_PI * f2 * t) +
            0.1f * sinf(2.0f * (float)M_PI * f3 * t);

        int16_t sample = (int16_t)(s * amp * env);

        buffer[i * 2] = sample;
        buffer[i * 2 + 1] = sample;
    }

    esp_codec_dev_set_out_vol(output_dev, 70.0);
    esp_codec_dev_write(output_dev, buffer, total_samples * 2 * sizeof(int16_t));

    heap_caps_free(buffer);
}