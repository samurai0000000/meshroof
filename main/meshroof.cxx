/*
 * meshroof.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <driver/gpio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory>
#include <iostream>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/usb_serial_jtag.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <meshroof.h>
#include <MeshRoof.hxx>
#include <MeshRoofShell.hxx>
#include "version.h"

#define BLINK_GPIO (gpio_num_t) 21

#define LED_TASK_STACK_SIZE            configMINIMAL_STACK_SIZE
#define LED_TASK_PRIORITY              (tskIDLE_PRIORITY + 1UL)
#define CONSOLE_TASK_STACK_SIZE        (6 * 1024)
#define CONSOLE_TASK_PRIORITY          (tskIDLE_PRIORITY + 2UL)
#define TCP_CONSOLE_TASK_STACK_SIZE    (6 * 1024)
#define TCP_CONSOLE_TASK_PRIORITY      (tskIDLE_PRIORITY + 3UL)
#define BINDER_TASK_STACK_SIZE         (2 * 1024)
#define BINDER_TASK_PRIORITY           (tskIDLE_PRIORITY + 4UL)

extern void serial_init(void);

using namespace std;

static const char *TAG = "meshroof";

shared_ptr<MeshRoof> meshroof = NULL;
static shared_ptr<MeshRoofShell> shell = NULL;
static shared_ptr<MeshRoofShell> shell2 = NULL;
static QueueHandle_t queue = NULL;

static string banner = "The meshroof firmware for ESP32-S3";
static string version = string("Version: ") + string(MYPROJECT_VERSION_STRING);
static string built = string("Built: ") +
    string(MYPROJECT_WHOAMI) + string("@") +
    string(MYPROJECT_HOSTNAME) + string(" ") + string(MYPROJECT_DATE);
static string copyright = string("Copyright (C) 2025, Charles Chiou");

static void led_task(__unused void *params)
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

static void console_task(__unused void *params)
{
    int ret;

    vTaskDelay(pdMS_TO_TICKS(1000));

    usb_printf("\n\x1b[2K");
    usb_printf("%s\n", shell->banner().c_str());
    usb_printf("%s\n", shell->version().c_str());
    usb_printf("%s\n", shell->built().c_str());
    usb_printf("-------------------------------------------\n");
    usb_printf("%s\n", shell->copyright().c_str());
    usb_printf("> ");

    for (;;) {
        do {
            ret = shell->process();
        } while (ret > 0);

        taskYIELD();
    }
}

static int tcp_fd = -1;

static void tcp_console_task(__unused void *params)
{
    int ret;

    for (;;) {
        if (tcp_fd == -1) {
            ret = xQueueReceive(queue, &tcp_fd, portMAX_DELAY);
            if ((ret == pdPASS) && (tcp_fd > 0)) {
                uint32_t ctx = 0x80000000 | tcp_fd;
                shell2->attach((void *) ctx, true);
            }
            continue;
        }

        ret = shell2->process();
        if (ret < 0) {
            shell2->detach();
            close(tcp_fd);
            tcp_fd = -1;
        }

        taskYIELD();
    }
}

static void binder_task(__unused void *params)
{
    int ret;
    int server_sock = -1;
    int client_sock = -1;
    struct sockaddr_in addr;
    socklen_t len;
    static const string msg = "Too many connected clients, bye!\n";

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        ESP_LOGE(TAG, "socket ret=%d", server_sock);
        goto done;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(16876);

    ret = bind(server_sock, (struct sockaddr *) &addr, sizeof(addr));
    if (ret != 0) {
        ESP_LOGE(TAG, "bind ret=%d", bind);
        goto done;
    }

    ret = listen(server_sock, 1);
    if (ret == -1) {
        ESP_LOGE(TAG, "listen ret=%d", ret);
        goto done;
    }

    for (;;) {
        len = sizeof(addr);
        client_sock = accept(server_sock, (struct sockaddr *) &addr, &len);
        if (client_sock == -1) {
            ESP_LOGE(TAG, "accpet ret=%d\n", client_sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (tcp_fd != -1) {
            write(client_sock, msg.c_str(), msg.size());
            close(client_sock);
            client_sock = -1;
        } else {
            ret = xQueueSend(queue, &client_sock, 0);
            if (ret != pdPASS) {
                write(client_sock, msg.c_str(), msg.size());
                close(client_sock);
                client_sock = -1;
            }
        }
    }

done:

    if (server_sock != -1) {
        close(server_sock);
    }

    for (;;) {
        ESP_LOGE(TAG, "binder_task is dead");
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

static void meshtastic_task(__unused void *params)
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
    meshroof->espWifi()->start();

    shell = make_shared<MeshRoofShell>();
    shell->setBanner(banner);
    shell->setVersion(version);
    shell->setBuilt(built);
    shell->setCopyright(copyright);
    shell->setClient(meshroof);
    shell->setNVM(meshroof);
    shell->attach((void *) 1);

    shell2 = make_shared<MeshRoofShell>();
    shell2->setBanner(banner);
    shell2->setVersion(version);
    shell2->setBuilt(built);
    shell2->setCopyright(copyright);
    shell2->setClient(meshroof);
    shell2->setNVM(meshroof);
    shell2->attach((void *) 0);
    shell2->setNoEcho(true);

    struct vprintf_callback cb = {
        .ctx = shell2.get(),
        .vprintf = SimpleShell::ctx_vprintf,
    };
    meshroof->addPrintfCallback(cb);

    queue = xQueueCreate(1, sizeof(int));

    xTaskCreatePinnedToCore(led_task,
                            "LedTask",
                            LED_TASK_STACK_SIZE,
                            NULL,
                            LED_TASK_PRIORITY,
                            NULL,
                            0);

    xTaskCreatePinnedToCore(console_task,
                            "ConsoleTask",
                            CONSOLE_TASK_STACK_SIZE,
                            NULL,
                            CONSOLE_TASK_PRIORITY,
                            NULL,
                            1);

    xTaskCreatePinnedToCore(tcp_console_task,
                            "TcpConsoleTask",
                            TCP_CONSOLE_TASK_STACK_SIZE,
                            NULL,
                            TCP_CONSOLE_TASK_PRIORITY,
                            NULL,
                            1);

    xTaskCreatePinnedToCore(binder_task,
                            "BinderTask",
                            BINDER_TASK_STACK_SIZE,
                            NULL,
                            BINDER_TASK_PRIORITY,
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
