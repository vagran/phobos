/*
 * /kernel/dev/uart/ser.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/chr/ser.h>

DefineDevFactory(SerialPort);

RegDevClass(SerialPort, "ser", Device::T_CHAR, "Serial interface");

SerialPort::SerialPort(Type type, u32 unit, u32 classID) :
	ChrDevice(type, unit, classID)
{
	switch (unit) {
	case 0:
		iobase = 0x3f8;
		break;
	case 1:
		iobase = 0x2f8;
		break;
	case 2:
		iobase = 0x3e8;
		break;
	case 3:
		iobase = 0x2e8;
		break;
	default:
		return;
	}
	SetSpeed(DEFAULT_SPEED);
	Initialize();
	devState = S_UP;
}

void
SerialPort::SetSpeed(int speed)
{
	static int speedTab[] = {
		2400,
		4800,
		9600,
		19200,
		38400,
		57600,
		115200
	};

	for (u32 i = 0; i < sizeof(speedTab) / sizeof(speedTab[0]); i++) {
		if (speed == speedTab[i]) {
			divisor = BASE_SPEED / speed;
			return;
		}
	}
	divisor = BASE_SPEED / speedTab[0];
}

void
SerialPort::Initialize()
{
	/* Turn off the interrupt */
	outb(iobase + UART_IER, 0);

	/* Set DLAB.  */
	outb(iobase + UART_LCR, UART_DLAB);

	/* Set the baud rate */
	outb(iobase + UART_DLL, divisor & 0xFF);
	outb(iobase + UART_DLH, divisor >> 8);

	/* Set the line status */
	u8 status = UART_8BITS_WORD | UART_NO_PARITY | UART_1_STOP_BIT;
	outb(iobase + UART_LCR, status);

	/* Enable the FIFO */
	outb(iobase + UART_FCR, UART_ENABLE_FIFO);

	/* Turn on DTR, RTS, and OUT2 */
	outb(iobase + UART_MCR, UART_ENABLE_MODEM);

	/* Drain the input buffer */
	u8 c;
	while (Getc(&c) == IOS_OK);
}

Device::IOStatus
SerialPort::Getc(u8 *c)
{
	u32 x = 0;
	if (im) {
		x = im->DisableIntr();
	}
	lock.Lock();
	if (inb(iobase + UART_LSR) & UART_DATA_READY) {
	    *c = inb(iobase + UART_RX);
	    lock.Unlock();
	    if (im) {
			im->RestoreIntr(x);
		}
	    return IOS_OK;
	}
	lock.Unlock();
	if (im) {
		im->RestoreIntr(x);
	}
	return IOS_NODATA;
}

Device::IOStatus
SerialPort::Putc(u8 c)
{
	if (c == '\n') {
		IOStatus status = Putc('\r');
		if (status != IOS_OK) {
			return status;
		}
	}
	u32 timeout = 100000;
	/* Wait until the transmitter holding register is empty */
	u32 x = 0;
	if (im) {
		x = im->DisableIntr();
	}
	lock.Lock();
	while (!inb(iobase + UART_LSR) & UART_EMPTY_TRANSMITTER) {
		if (!--timeout) {
			/* There is something wrong. But what can I do? */
			lock.Unlock();
			if (im) {
				im->RestoreIntr(x);
			}
			return IOS_ERROR;
		}
	}
	outb(iobase + UART_TX, c);
	lock.Unlock();
	if (im) {
		im->RestoreIntr(x);
	}
	return IOS_OK;
}
