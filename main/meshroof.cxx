/*
 * meshroof.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <driver/gpio.h>
#include <memory>
#include <iostream>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/usb_serial_jtag.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <meshroof.h>
#include <MeshRoof.hxx>
#include "version.h"

#define BLINK_GPIO (gpio_num_t) 21

#define LED_TASK_STACK_SIZE            configMINIMAL_STACK_SIZE
#define LED_TASK_PRIORITY              (tskIDLE_PRIORITY + 1UL)
#define CONSOLE_TASK_STACK_SIZE        (4 * 1024)
#define CONSOLE_TASK_PRIORITY          (tskIDLE_PRIORITY + 2UL)

extern void serial_init(void);

using namespace std;

static const char *TAG = "meshroof";

shared_ptr<MeshRoof> meshroof = NULL;

void led_task(__unused void *params)
{
    bool led_on = false;

    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    for (;;) {
        gpio_set_level(BLINK_GPIO, led_on);
        led_on = !led_on;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void console_task(__unused void *params)
{
    int ret;

    vTaskDelay(pdMS_TO_TICKS(1000));

    usb_printf("\n\x1b[2K");
    usb_printf("The meshroof firmware for ESP32-S3\n");
    usb_printf("Version: %s\n", MYPROJECT_VERSION_STRING);
    usb_printf("Built: %s@%s %s\n",
               MYPROJECT_WHOAMI, MYPROJECT_HOSTNAME, MYPROJECT_DATE);
    usb_printf("-------------------------------------------\n");
    usb_printf("Copyright (C) 2025, Charles Chiou\n");
    usb_printf("> ");

    for (;;) {
        while (usb_rx_ready() > 0) {
            ret = shell_process();
            if (ret <= 0) {
                break;
            }
        }

        taskYIELD();
    }
}

void meshtastic_task(__unused void *params)
{
    int ret;
    time_t now, last_want_config, last_heartbeat;

    now = time(NULL);
    last_heartbeat = now;
    last_want_config = 0;

    for (;;) {
        now = time(NULL);

        if (!meshroof->isConnected() && ((now - last_want_config) >= 5)) {
            ret = meshroof->sendWantConfig();
            if (ret == false) {
                usb_printf("sendWantConfig failed!\n");
            }

            last_want_config = now;
        } else if (meshroof->isConnected()) {
            last_want_config = now;
        }

        if (meshroof->isConnected() && ((now - last_heartbeat) >= 60)) {
            ret = meshroof->sendHeartbeat();
            if (ret == false) {
                usb_printf("sendHeartbeat failed!\n");
            }

            last_heartbeat = now;
        }

        while (serial_rx_ready() > 0) {
            ret = mt_serial_process(&meshroof->_mtc, 0);
            if (ret != 0) {
                usb_printf("mt_serial_process failed!\n");
                break;
            }
        }

        taskYIELD();
    }
}

extern "C" void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "meshroof.cxx app_main");

    serial_init();

    err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) ||
        (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    meshroof = make_shared<MeshRoof>();
    meshroof->setClient(meshroof);
    meshroof->sendDisconnect();
    if (meshroof->loadNvm() == false) {
        meshroof->saveNvm();  // Create a default
    }
    meshroof->applyNvmToHomeChat();

    shell_init();

    xTaskCreatePinnedToCore(led_task,
                            "LedTask",
                            LED_TASK_STACK_SIZE,
                            NULL,
                            LED_TASK_PRIORITY,
                            NULL,
                            1);

    xTaskCreatePinnedToCore(console_task,
                            "ConsoleTask",
                            CONSOLE_TASK_STACK_SIZE,
                            NULL,
                            CONSOLE_TASK_PRIORITY,
                            NULL,
                            1);

    meshtastic_task(NULL);
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
