/*
 * meshroof.h
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHROOF_H
#define MESHROOF_H

#if !defined(EXTERN_C_BEGIN)
#if defined(__cplusplus)
#define EXTERN_C_BEGIN extern "C" {
#else
#define EXTERN_C_BEGIN
#endif
#endif

#if !defined(EXTERN_C_END)
#if defined(__cplusplus)
#define EXTERN_C_END }
#else
#define EXTERN_C_END
#endif
#endif

EXTERN_C_BEGIN

extern void led_init(void);
extern void led_set(bool on);

extern void serial_init(void);
extern int usb_printf(const char *fmt, ...);
extern int usb_rx_ready(void);
extern int usb_rx_read(uint8_t *data, size_t size);

extern void shell_init(void);
extern void shell_process(void);

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
