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
#include <meshroof.h>
#include <MeshRoof.hxx>
#include "version.h"

#define BLINK_GPIO (gpio_num_t) 21

extern void serial_init(void);

using namespace std;

static const char *TAG = "meshroof";

shared_ptr<MeshRoof> meshroof = NULL;

extern "C" void app_main(void)
{
    int ret;
    bool led_on = false;
    uint32_t now, last_flip, last_want_config, last_heartbeat;

    ESP_LOGI(TAG, "meshroof.cxx app_main");

    serial_init();

    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    meshroof = make_shared<MeshRoof>();
    meshroof->sendDisconnect();
    vTaskDelay(200 / portTICK_PERIOD_MS);

    usb_printf("\n\x1b[2K");
    usb_printf("The meshroof firmware for ESP32-S3\n");
    usb_printf("Version: %s\n", MYPROJECT_VERSION_STRING);
    usb_printf("Built: %s@%s %s\n",
               MYPROJECT_WHOAMI, MYPROJECT_HOSTNAME, MYPROJECT_DATE);
    usb_printf("-------------------------------------------\n");
    usb_printf("Copyright (C) 2025, Charles Chiou\n");

    shell_init();

    now = (uint32_t) (esp_timer_get_time() / 1000000);
    last_flip = now;
    last_heartbeat = now;
    last_want_config = 0;

    for (;;) {
        now = (uint32_t) (esp_timer_get_time() / 1000000);

        if ((now - last_flip) >= 1) {
            led_on = !led_on;
            gpio_set_level(BLINK_GPIO, led_on);
            last_flip = now;
        }

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

        while (usb_rx_ready() > 0) {
            ret = shell_process();
            if (ret <= 0) {
                break;
            }
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

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
