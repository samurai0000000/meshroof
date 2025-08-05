/*
 * serial.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <fcntl.h>
#include <sys/select.h>
#include <driver/uart.h>
#include <driver/uart_vfs.h>
#include <driver/usb_serial_jtag.h>
#include <esp_console.h>
#include <esp_vfs_dev.h>
#include <esp_log.h>
#include <sdkconfig.h>
#include <meshroof.h>

#define SERIAL_PBUF_SIZE  256

#define BUF_SIZE 1024

static const char *TAG = "serial";

void serial_init(void)
{
    int ret;
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .rx_buffer_size = BUF_SIZE,
        .tx_buffer_size = BUF_SIZE,
    };

    ret = uart_driver_install(UART_NUM_0, 2 * 1024, 0, 0, NULL, 0);
    if (ret!= ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed!");
        goto done;
	}

    uart_param_config(UART_NUM_0, &uart_config);
    uart_vfs_dev_use_driver(UART_NUM_0);

    ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "usb_serial_jtag_driver_install failed!");
        goto done;
    }

done:

    return;
}

int usb_printf(const char *fmt, ...)
{
    int ret;
    va_list ap;
    char pbuf[SERIAL_PBUF_SIZE];
    int i;

    va_start(ap, fmt);
    ret = vsnprintf(pbuf, SERIAL_PBUF_SIZE - 1, fmt, ap);
    va_end(ap);

    for (i = 0; i < ret; i++) {
        if (pbuf[i] == '\n') {
            while (usb_serial_jtag_write_bytes("\r", 1, 0) != 1);
        }
        while (usb_serial_jtag_write_bytes(pbuf + i, 1, 0) != 1);
    }

    return ret;
}

int usb_rx_ready(void)
{
    return 1;
}

int usb_rx_read(uint8_t *data, size_t size)
{
    return usb_serial_jtag_read_bytes(data, size,
                                      50 / portTICK_PERIOD_MS);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
