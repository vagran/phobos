/*
 * /kernel/init/init.c
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <boot.h>

#define SERIAL_DEBUG

#ifdef SERIAL_DEBUG
#include <dev/uart/uart.h>

#define SERIAL_PORT		0x3f8

static void
dbg_putc(i32 c)
{
	unsigned int timeout = 100000;

	if (c=='\n') {
		dbg_putc('\r');
	}
	/* Wait until the transmitter holding register is empty.  */
	while (!(inb(SERIAL_PORT + UART_LSR) & UART_EMPTY_TRANSMITTER)) {
		if (!--timeout) {
			return;
		}
	}
	outb(c, SERIAL_PORT + UART_TX);
}

static void
dbg_puts(char *s)
{
	while (*s) {
		dbg_putc(*s++);
	}
}

static void
dbg_print_num(u32 num, u32 base)
{
	if (num >= base) {
		dbg_print_num(num / base, base);
		num = num % base;
	}
	if (num < 10) {
		dbg_putc('0' + num);
	} else {
		dbg_putc('a' + num - 10);
	}
}

void
dbg_printf(char *fmt,...)
{
	va_list va;

	va_start(va, fmt);
	while (*fmt) {
		if (*fmt != '%') {
			dbg_putc(*fmt++);
		} else {
			fmt++;
			if (!*fmt) {
				dbg_putc('%');
				break;
			}
			switch (*fmt) {
			case 'd': {
				u32 x = va_arg(va, u32);
				dbg_print_num(x, 10);
				break;
			}
			case 'x': {
				u32 x = va_arg(va, u32);
				dbg_print_num(x, 16);
				break;
			}
			case 's': {
				char *s = va_arg(va, char *);
				dbg_puts(s);
				break;
			}
			default:
				dbg_putc(*fmt);
			}
			fmt++;
		}
	}
}

#define TRACE(s,...)	dbg_printf(s, ## __VA_ARGS__)
#else /* SERIAL_DEBUG */

#define TRACE(s,...)

#endif /* SERIAL_DEBUG */


u32
Bootstrap(u32 mbSignature, MBInfo *mbi)
{
	u8 *p;

	/* zero bootstrap bss */
	for (p = &_bootbss; p < &_eboot; p++) {
		*p = 0;
	}
	TRACE("Bootstrapping started\n");

	hlt();
	return 0;
}
