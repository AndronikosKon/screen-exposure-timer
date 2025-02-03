/*
 * MIT License
 *
 * Copyright (c) 2021 Juan Schiavoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdbool.h>
#include <stdint.h>
#include "gpio_task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "musical_score_encoder.h"
#include "rotary_encoder.h"
#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "HD44780.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp32_driver_nextion/nextion.h"
#include "esp32_driver_nextion/page.h"
#include "esp32_driver_nextion/component.h"

#define TAG "app"

#define LCD_ADDR 0x27
#define SDA_PIN 4
#define SCL_PIN 3
#define LCD_COLS 16
#define LCD_ROWS 2

#define RELAY_PIN 5
nvs_handle_t my_nvs_handle;
static TaskHandle_t task_handle_user_interface;

int time = 10;
static void callback_touch_event(nextion_on_touch_event_t event);
static void process_callback_queue(void *pvParameters);

bool isExposing = false;

void countdownTask(void *pvParameters)
{
    gpio_set_level(RELAY_PIN, 1);

    int initialTime = time;
    nextion_t *nextion_handle = (nextion_t *)pvParameters;
    while (time > 0)
    {
        if (!isExposing)
        {
            gpio_set_level(RELAY_PIN, 0);
            time = initialTime;
            isExposing = false;
            int minutes = time / 60;
            int seconds = time % 60;
            nextion_component_set_value(nextion_handle, "n0", minutes);
            nextion_component_set_value(nextion_handle, "n1", seconds);
            nextion_component_set_text(nextion_handle, "b0", "Start Exposure");
            nextion_component_set_visibility(nextion_handle, "b1", true);
            nextion_component_set_visibility(nextion_handle, "b2", true);
            nextion_component_set_visibility(nextion_handle, "b3", true);
            nextion_component_set_visibility(nextion_handle, "b4", true);
            nextion_component_set_visibility(nextion_handle, "b5", true);
            nextion_component_set_visibility(nextion_handle, "b6", true);
            nextion_component_set_visibility(nextion_handle, "b7", true);
            nextion_component_set_visibility(nextion_handle, "b8", true);
            nextion_component_set_visibility(nextion_handle, "j0", false);
            nextion_component_set_value(nextion_handle, "j0", 0);

            vTaskDelete(NULL);
        }
        time--;
        nextion_component_set_value(nextion_handle, "j0", ((float)(initialTime - time)) / initialTime * 100);
        int minutes = time / 60;
        int seconds = time % 60;
        nextion_component_set_value(nextion_handle, "n0", minutes);
        nextion_component_set_value(nextion_handle, "n1", seconds);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    gpio_set_level(RELAY_PIN, 0);
    time = initialTime;
    isExposing = false;
    int minutes = time / 60;
    int seconds = time % 60;
    nextion_component_set_value(nextion_handle, "n0", minutes);
    nextion_component_set_value(nextion_handle, "n1", seconds);
    nextion_component_set_text(nextion_handle, "b0", "Start Exposure");
    nextion_component_set_visibility(nextion_handle, "b1", true);
    nextion_component_set_visibility(nextion_handle, "b2", true);
    nextion_component_set_visibility(nextion_handle, "b3", true);
    nextion_component_set_visibility(nextion_handle, "b4", true);
    nextion_component_set_visibility(nextion_handle, "b5", true);
    nextion_component_set_visibility(nextion_handle, "b6", true);
    nextion_component_set_visibility(nextion_handle, "b7", true);
    nextion_component_set_visibility(nextion_handle, "b8", true);
    nextion_component_set_visibility(nextion_handle, "j0", false);
    nextion_component_set_value(nextion_handle, "j0", 0);
    play_win(1);
    vTaskDelete(NULL);
}

// static void button_callback(void *arg)
// {
//     ESP_LOGI(TAG, "Button pressed");
//     if (mode == HOVER_START)
//     {
//         mode = COUNTDOWN;
//         ESP_LOGI(TAG, "Mode changed to COUNTDOWN");
//         xTaskCreate(countdownTask, "Countdown Task", 2048, NULL, 5, NULL);
//         gpio_set_level(RELAY_PIN, 1);
//     }
//     else if (mode == EDIT_TIME)
//     {
//         mode = HOVER_TIME;
//         nvs_set_i32(my_nvs_handle, "time", time);
//         ESP_LOGI(TAG, "Mode changed to HOVER_TIME");
//     }
//     else if (mode == HOVER_TIME)
//     {
//         mode = EDIT_TIME;
//         ESP_LOGI(TAG, "Mode changed to EDIT_TIME");
//     }
//     else if (mode == COUNTDOWN)
//     {
//         mode = HOVER_START;
//         ESP_LOGI(TAG, "Mode changed to HOVER_START");
//     }
// }

// static void event_callback(rotenc_event_t event)
// {
//     // ESP_LOGI(TAG, "Event: position %d, direction %s", (int)event.position,
//     //          event.direction ? (event.direction == ROTENC_CW ? "CW" : "CCW") : "NOT_SET");
//     if (mode == EDIT_TIME)
//     {
//         if (event.direction == ROTENC_CW)
//         {
//             time += 1;
//         }
//         else
//         {
//             time -= 1;
//         }
//         ESP_LOGI(TAG, "Time: %d", time);
//     }
//     else if (mode == HOVER_TIME)
//     {
//         mode = HOVER_START;
//     }
//     else if (mode == HOVER_START)
//     {
//         mode = HOVER_TIME;
//     }
//     rotenc_reset(handleref);
// }

void app_main()
{
    // Verify that the GPIO ISR service is installed, before initializing the driver.

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = nvs_open("storage", NVS_READWRITE, &my_nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }

    // Read
    int32_t saved_time = 0; // value will default to 0, if not set yet in NVS
    err = nvs_get_i32(my_nvs_handle, "time", &saved_time);
    switch (err)
    {
    case ESP_OK:
        time = saved_time;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "The value is not initialized yet!\n");
        err = nvs_set_i32(my_nvs_handle, "time", time);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) writing!\n", esp_err_to_name(err));
        }
        break;
    default:
        // ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
    }

    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    // Initialize the GPIOs for the relay
    gpio_reset_pin(RELAY_PIN);
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_PIN, 0);
    // Initialize UART.
    nextion_t *nextion_handle = nextion_driver_install(UART_NUM_1,
                                                       115200,
                                                       GPIO_NUM_9,
                                                       GPIO_NUM_10);

    // Do basic configuration.
    nextion_init(nextion_handle);

    // Set a callback for touch events.
    nextion_event_callback_set_on_touch(nextion_handle,
                                        callback_touch_event);

    // Go to page with id 0.
    nextion_page_set(nextion_handle, "0");
    int minutes = time / 60;
    int seconds = time % 60;
    nextion_component_set_value(nextion_handle, "n0", minutes);
    nextion_component_set_value(nextion_handle, "n1", seconds);

    // Start a task that will handle touch notifications.
    xTaskCreate(process_callback_queue,
                "user_interface",
                2048,
                (void *)nextion_handle,
                5,
                &task_handle_user_interface);

    ESP_LOGI(TAG, "waiting for button to be pressed");

    vTaskDelay(portMAX_DELAY);
}

static void callback_touch_event(nextion_on_touch_event_t event)
{
    ESP_LOGI(TAG, "page_id: %d, component_id: %d, state: %d",
             event.page_id, event.component_id, event.state);

    if (event.page_id == 0 && event.state == NEXTION_TOUCH_RELEASED)
    {
        ESP_LOGI(TAG, "button pressed");

        xTaskNotify(task_handle_user_interface,
                    event.component_id,
                    eSetValueWithOverwrite);
    }
}

[[noreturn]] static void process_callback_queue(void *pvParameters)
{
    const uint8_t MAX_TEXT_LENGTH = 50;

    nextion_t *nextion_handle = (nextion_t *)pvParameters;
    char text_buffer[MAX_TEXT_LENGTH];
    size_t text_length = MAX_TEXT_LENGTH;
    int32_t number;

    for (;;)
    {
        int button = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        switch (button)
        {
        case 2:
            if (isExposing)
            {
                nextion_component_set_text(nextion_handle, "b0", "Start Exposure");
                nextion_component_set_visibility(nextion_handle, "b1", true);
                nextion_component_set_visibility(nextion_handle, "b2", true);
                nextion_component_set_visibility(nextion_handle, "b3", true);
                nextion_component_set_visibility(nextion_handle, "b4", true);
                nextion_component_set_visibility(nextion_handle, "b5", true);
                nextion_component_set_visibility(nextion_handle, "b6", true);
                nextion_component_set_visibility(nextion_handle, "b7", true);
                nextion_component_set_visibility(nextion_handle, "b8", true);
                nextion_component_set_visibility(nextion_handle, "j0", false);
                isExposing = !isExposing;
            }
            else
            {
                nextion_component_set_text(nextion_handle, "b0", "Stop Exposure");
                nextion_component_set_visibility(nextion_handle, "b1", false);
                nextion_component_set_visibility(nextion_handle, "b2", false);
                nextion_component_set_visibility(nextion_handle, "b3", false);
                nextion_component_set_visibility(nextion_handle, "b4", false);
                nextion_component_set_visibility(nextion_handle, "b5", false);
                nextion_component_set_visibility(nextion_handle, "b6", false);
                nextion_component_set_visibility(nextion_handle, "b7", false);
                nextion_component_set_visibility(nextion_handle, "b8", false);
                nextion_component_set_visibility(nextion_handle, "j0", true);
                isExposing = !isExposing;
                xTaskCreate(countdownTask, "Countdown Task", 2048, (void *)nextion_handle, 5, NULL);
            }
            break;
        case 4:
            time += 60;
            break;
        case 5:
            time -= 60;
            break;
        case 6:
            time += 1;
            break;
        case 7:
            time -= 1;
            break;
        case 8:
            time = 300;
            break;
        case 9:
            time = 600;
            break;
        case 10:
            time = 900;
            break;
        case 11:
            time = 1200;
            break;
        default:
            break;
        }
        ESP_LOGI(TAG, "Time: %d", time);
        nvs_set_i32(my_nvs_handle, "time", time);
        int minutes = time / 60;
        int seconds = time % 60;
        nextion_component_set_value(nextion_handle, "n0", minutes);
        nextion_component_set_value(nextion_handle, "n1", seconds);

        // Get the text value from a component.
        nextion_component_get_text(nextion_handle,
                                   "value_text",
                                   text_buffer,
                                   &text_length);

        // Get the integer value from a component.
        nextion_component_get_value(nextion_handle,
                                    "value_number",
                                    &number);

        ESP_LOGI(TAG, "text: %s", text_buffer);
        ESP_LOGI(TAG, "number: %lu", number);
    }
}
