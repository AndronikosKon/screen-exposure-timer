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
#define TAG "app"

#define LCD_ADDR 0x27
#define SDA_PIN 4
#define SCL_PIN 3
#define LCD_COLS 16
#define LCD_ROWS 2

#define RELAY_PIN 5
nvs_handle_t my_nvs_handle;
rotenc_handle_t *handleref;

enum
{
    HOVER_TIME,
    HOVER_START,
    EDIT_TIME,
    COUNTDOWN
};

int time = 10;

int mode = HOVER_TIME;

void countdownTask(void *param)
{
    int initialTime = time;
    int count = 0;
    while (time > 0)
    {
        if (mode != COUNTDOWN)
        {
            gpio_set_level(RELAY_PIN, 0);
            time = initialTime;
            vTaskDelete(NULL);
        }
        if (count == 9)
        {
            count = 0;
            time--;
        }
        else
        {
            count++;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    gpio_set_level(RELAY_PIN, 0);
    time = initialTime;

    mode = HOVER_START;
    play_win(1);
    vTaskDelete(NULL);
}

static void button_callback(void *arg)
{
    ESP_LOGI(TAG, "Button pressed");
    if (mode == HOVER_START)
    {
        mode = COUNTDOWN;
        ESP_LOGI(TAG, "Mode changed to COUNTDOWN");
        xTaskCreate(countdownTask, "Countdown Task", 2048, NULL, 5, NULL);
        gpio_set_level(RELAY_PIN, 1);
    }
    else if (mode == EDIT_TIME)
    {
        mode = HOVER_TIME;
        nvs_set_i32(my_nvs_handle, "time", time);
        ESP_LOGI(TAG, "Mode changed to HOVER_TIME");
    }
    else if (mode == HOVER_TIME)
    {
        mode = EDIT_TIME;
        ESP_LOGI(TAG, "Mode changed to EDIT_TIME");
    }
    else if (mode == COUNTDOWN)
    {
        mode = HOVER_START;
        ESP_LOGI(TAG, "Mode changed to HOVER_START");
    }
}

static void event_callback(rotenc_event_t event)
{
    // ESP_LOGI(TAG, "Event: position %d, direction %s", (int)event.position,
    //          event.direction ? (event.direction == ROTENC_CW ? "CW" : "CCW") : "NOT_SET");
    if (mode == EDIT_TIME)
    {
        if (event.direction == ROTENC_CW)
        {
            time += 1;
        }
        else
        {
            time -= 1;
        }
        ESP_LOGI(TAG, "Time: %d", time);
    }
    else if (mode == HOVER_TIME)
    {
        mode = HOVER_START;
    }
    else if (mode == HOVER_START)
    {
        mode = HOVER_TIME;
    }
    rotenc_reset(handleref);
}

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

    // Open
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");

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
        if(err != ESP_OK){
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

    // Initialize the handle instance of the rotary device,
    // by default it uses 1 mS for the debounce time.
    rotenc_handle_t handle = {0};
    handleref = &handle;
    ESP_ERROR_CHECK(rotenc_init(&handle,
                                CONFIG_ROT_ENC_CLK_GPIO,
                                CONFIG_ROT_ENC_DTA_GPIO,
                                CONFIG_ROT_ENC_DEBOUNCE));

    ESP_ERROR_CHECK(rotenc_init_button(&handle,
                                       CONFIG_ROT_ENC_BUTTON_GPIO,
                                       CONFIG_ROT_ENC_BUTTON_DEBOUNCE,
                                       button_callback));

    ESP_LOGI(TAG, "Report mode by function callback");
    ESP_ERROR_CHECK(rotenc_set_event_callback(&handle, event_callback));



    LCD_init(LCD_ADDR, SDA_PIN, SCL_PIN, LCD_COLS, LCD_ROWS);

    LCD_home();
    LCD_clearScreen();
    LCD_setCursor(0, 0);
    char txtBuf[22];
    uint8_t min = time / 60;
    uint8_t sec = time % 60;
    sprintf(txtBuf, "> %d m %d s", min, sec);
    LCD_writeStr(txtBuf);

    LCD_setCursor(0, 1);
    LCD_writeStr("Start exposure");

    bool cursorOn = true;
    int count = 0;

    int prevTime = time;
    int prevMode = mode;
    while (1)
    {
        if (prevMode != mode)
        {
            prevMode = mode;
            if (mode == COUNTDOWN)
            {
                count = 0;
            }
        }

        LCD_home();

        if (mode == HOVER_TIME)
        {
            LCD_setCursor(0, 0);
            uint8_t min = time / 60;
            uint8_t sec = time % 60;
            sprintf(txtBuf, "> %d m %d s      ", min, sec);
            LCD_writeStr(txtBuf);
            LCD_setCursor(0, 1);
            LCD_writeStr("Start exposure  ");
        }
        else if (mode == EDIT_TIME)
        {
            LCD_setCursor(2, 0);
            uint8_t min = time / 60;
            uint8_t sec = time % 60;
            sprintf(txtBuf, "%d m %d s", min, sec);
            LCD_writeStr(txtBuf);
            LCD_setCursor(0, 1);
            LCD_writeStr("Start exposure  ");
        }
        else if (mode == HOVER_START)
        {
            LCD_setCursor(0, 0);
            uint8_t min = time / 60;
            uint8_t sec = time % 60;
            sprintf(txtBuf, "%d m %d s      ", min, sec);
            LCD_writeStr(txtBuf);
            LCD_setCursor(0, 1);
            LCD_writeStr("> Start exposure");
        }
        else
        {
            LCD_setCursor(0, 0);
            uint8_t min = time / 60;
            uint8_t sec = time % 60;
            sprintf(txtBuf, "%d m %d s", min, sec);
            LCD_writeStr(txtBuf);
        }
        if ((count == 8 || count == 16) && mode == EDIT_TIME)
        {
            LCD_setCursor(0, 0);
            if (cursorOn)
            {
                LCD_writeStr("  ");
            }
            else
            {
                LCD_writeStr("> ");
            }
            cursorOn = !cursorOn;
        }

        if (count == 17)
        {
            LCD_setCursor(0, 1);
            if (mode == COUNTDOWN)
            {
                LCD_writeStr("                ");
            }
            count = 0;
        }

        if (mode == COUNTDOWN)
        {
            LCD_setCursor(count, 1);
            LCD_writeChar('*');
        }

        count++;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
