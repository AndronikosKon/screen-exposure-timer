/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "musical_score_encoder.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "musical_score_encoder.h"

#define RMT_BUZZER_RESOLUTION_HZ 1000000 // 1MHz resolution
#define RMT_BUZZER_GPIO_NUM 6

static const char *TAG = "example";

float tempo = 285.71 / 2; // 286 ms per beat

/**
 * @brief Musical Score: Beethoven's Ode to joy
 */
static const buzzer_musical_score_t score[] = {
    {165, 1},
    {440, 1},
    {277, 1},
    {330, 1},
    {880, 1},
    {554, 1},
    {659, 3},
    {554, 3},
    {175, 1},
    {440, 1},
    {262, 1},
    {349, 1},
    {880, 1},
    {523, 1},
    {698, 3},
    {523, 3},
    {147, 1},
    {494, 1},
    {294, 1},
    {392, 1},
    {988, 1},
    {587, 1},
    {784, 3},
    {784, 1},
    {784, 1},
    {784, 1},
    {1760, 6},
};

void play_win() {
        ESP_LOGI(TAG, "Create RMT TX channel");
        rmt_channel_handle_t buzzer_chan = NULL;
        rmt_tx_channel_config_t tx_chan_config = {
            .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
            .gpio_num = RMT_BUZZER_GPIO_NUM,
            .mem_block_symbols = 64,
            .resolution_hz = RMT_BUZZER_RESOLUTION_HZ,
            .trans_queue_depth = 10, // set the maximum number of transactions that can pend in the background
        };
        ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &buzzer_chan));

        ESP_LOGI(TAG, "Install musical score encoder");
        rmt_encoder_handle_t score_encoder = NULL;
        musical_score_encoder_config_t encoder_config = {
            .resolution = RMT_BUZZER_RESOLUTION_HZ};
        ESP_ERROR_CHECK(rmt_new_musical_score_encoder(&encoder_config, &score_encoder));

        ESP_LOGI(TAG, "Enable RMT TX channel");
        ESP_ERROR_CHECK(rmt_enable(buzzer_chan));
        ESP_LOGI(TAG, "Playing Beethoven's Ode to joy...");

        for (size_t i = 0; i < sizeof(score) / sizeof(score[0]); i++)
        {
            rmt_transmit_config_t tx_config = {
                .loop_count = score[i].duration_ms * tempo * score[i].freq_hz / 1000,
            };
            ESP_ERROR_CHECK(rmt_transmit(buzzer_chan, score_encoder, &score[i], sizeof(buzzer_musical_score_t), &tx_config));
        }
}

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *copy_encoder;
    uint32_t resolution;
} rmt_musical_score_encoder_t;

static size_t rmt_encode_musical_score(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_musical_score_encoder_t *score_encoder = __containerof(encoder, rmt_musical_score_encoder_t, base);
    rmt_encoder_handle_t copy_encoder = score_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    buzzer_musical_score_t *score = (buzzer_musical_score_t *)primary_data;
    uint32_t rmt_raw_symbol_duration = score_encoder->resolution / score->freq_hz / 2;
    rmt_symbol_word_t musical_score_rmt_symbol = {
        .level0 = 0,
        .duration0 = rmt_raw_symbol_duration,
        .level1 = 1,
        .duration1 = rmt_raw_symbol_duration,
    };
    size_t encoded_symbols = copy_encoder->encode(copy_encoder, channel, &musical_score_rmt_symbol, sizeof(musical_score_rmt_symbol), &session_state);
    *ret_state = session_state;
    return encoded_symbols;
}

static esp_err_t rmt_del_musical_score_encoder(rmt_encoder_t *encoder)
{
    rmt_musical_score_encoder_t *score_encoder = __containerof(encoder, rmt_musical_score_encoder_t, base);
    rmt_del_encoder(score_encoder->copy_encoder);
    free(score_encoder);
    return ESP_OK;
}

static esp_err_t rmt_musical_score_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_musical_score_encoder_t *score_encoder = __containerof(encoder, rmt_musical_score_encoder_t, base);
    rmt_encoder_reset(score_encoder->copy_encoder);
    return ESP_OK;
}

esp_err_t rmt_new_musical_score_encoder(const musical_score_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_musical_score_encoder_t *score_encoder = NULL;
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    score_encoder = calloc(1, sizeof(rmt_musical_score_encoder_t));
    ESP_GOTO_ON_FALSE(score_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for musical score encoder");
    score_encoder->base.encode = rmt_encode_musical_score;
    score_encoder->base.del = rmt_del_musical_score_encoder;
    score_encoder->base.reset = rmt_musical_score_encoder_reset;
    score_encoder->resolution = config->resolution;
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &score_encoder->copy_encoder), err, TAG, "create copy encoder failed");
    *ret_encoder = &score_encoder->base;
    return ESP_OK;
err:
    if (score_encoder) {
        if (score_encoder->copy_encoder) {
            rmt_del_encoder(score_encoder->copy_encoder);
        }
        free(score_encoder);
    }
    return ret;
}
