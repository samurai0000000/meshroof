/*
 * serial.h
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef SERIAL_H
#define SERIAL_H

EXTERN_C_BEGIN

typedef struct uart_inst uart_inst_t;

extern void serial_init(void);

extern int usb_tx_write(const uint8_t *data, size_t size);
extern int usb_printf(const char *format, ...);
extern int usb_vprintf(const char *format, va_list ap);
extern int usb_rx_ready(void);
extern int usb_rx_read(uint8_t *data, size_t size);
extern int usb_rx_read_timeout(uint8_t *data, size_t size, unsigned int ticks);

extern int serial_write(const void *buf, size_t len);
extern int serial_rx_ready(void);
extern int serial_read(void *buf, size_t len);

EXTERN_C_END

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
