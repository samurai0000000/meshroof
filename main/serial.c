/*
 * serial.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <driver/uart_vfs.h>
#include <driver/usb_serial_jtag.h>
#include <esp_console.h>
#include <esp_vfs_dev.h>
#include <esp_log.h>
#include <sdkconfig.h>
#include <meshroof.h>

#define SERIAL_PBUF_SIZE  512

#define SERIAL_RX_PIN GPIO_NUM_44
#define SERIAL_TX_PIN GPIO_NUM_43
#define UART_BUF_SIZE 512

static const char *TAG = "serial";

static int fd = -1;

void serial_init(void)
{
    int ret;

    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .rx_buffer_size = UART_BUF_SIZE,
        .tx_buffer_size = UART_BUF_SIZE,
    };

    // Set up the USB/Serial/JTAG driver first (console over USB CDC)
    ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "usb_serial_jtag_driver_install failed!");
        goto done;
    }

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ret = uart_driver_install(UART_NUM_0, UART_BUF_SIZE * 2,
                              0, 0, NULL, 0);
    if (ret!= ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed!");
        goto done;
	}

    ret = uart_param_config(UART_NUM_0, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed!");
        goto done;
    }

    uart_vfs_dev_use_driver(UART_NUM_0);

    ret = uart_set_pin(UART_NUM_0, SERIAL_TX_PIN, SERIAL_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed!");
    }

    fd = open("/dev/uart/0", O_RDWR);
    if (fd == -1) {
        ESP_LOGE(TAG, "open /dev/uart/0 failed!");
        goto done;
    }

done:

    return;
}

int usb_tx_write(const uint8_t *data, size_t size)
{
    if (usb_serial_jtag_is_connected() == false) {
        return size;
    }

    return usb_serial_jtag_write_bytes(data, size, 0);
}

int usb_printf(const char *format, ...)
{
    int ret = 0;
    va_list ap;

    va_start(ap, format);
    ret = usb_vprintf(format, ap);
    va_end(ap);

    return ret;
}

int usb_vprintf(const char *format, va_list ap)
{
    int ret = 0;
    char pbuf[SERIAL_PBUF_SIZE];
    int i;

    ret = vsnprintf(pbuf, SERIAL_PBUF_SIZE - 1, format, ap);

    if (usb_serial_jtag_is_connected() == false) {
        goto done;
    }

    for (i = 0; i < ret; i++) {
        if (pbuf[i] == '\n') {
            do {
                if (usb_serial_jtag_is_connected() == false) {
                    break;
                }
            } while (usb_serial_jtag_write_bytes("\r", 1, 0) != 1);
        }
        do {
            if (usb_serial_jtag_is_connected() == false) {
                break;
            }
        } while (usb_serial_jtag_write_bytes(pbuf + i, 1, 0) != 1);
    }

done:

    return ret;
}

int usb_rx_ready(void)
{
    return usb_serial_jtag_is_connected();
}

int usb_rx_read_timeout(uint8_t *data, size_t size, unsigned int ticks)
{
    return usb_serial_jtag_read_bytes(data, size, ticks);
}

int serial_write(const void *buf, size_t len)
{
    return uart_write_bytes(UART_NUM_0, buf, len);
}

int serial_rx_ready(void)
{
    int ret = 0;
    fd_set rfds;
    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 0,
    };

    if (fd == -1) {
        ret = -1;
        goto done;
    }

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    ret = select(fd + 1, &rfds, NULL, NULL, &timeout);
    if (ret == -1) {
        ESP_LOGE(TAG, "select ret=%d errno=%d", ret, errno);
    }

done:

    return ret;
}

int serial_read(void *buf, size_t len)
{
    int ret = 0;

    ret = serial_rx_ready();
    if (ret > 0) {
        ret = uart_read_bytes(UART_NUM_0, buf, len, 0);
    }

    return ret;
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
