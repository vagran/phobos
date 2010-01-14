/*
 * /kernel/dev/chr/syscons.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/chr/syscons.h>
#include <dev/uart/uart.h>

/* default system console */
SysConsole *sysCons;

#define SERIAL_SPEED 115200

/*
 * System serial interface used on early boot stage. Must be as
 * simple as possible and independent on other system components.
 */

SysConsole::SysSerial::SysSerial(Type type, u32 unit, u32 classID) :
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
	SetSpeed(SERIAL_SPEED);
	Initialize();
	devState = S_UP;
}

void
SysConsole::SysSerial::SetSpeed(int speed)
{
	int speedTab[] = {
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
			divisor = 115200 / speed;
			return;
		}
	}
	divisor = 115200 / speedTab[0];
}

void
SysConsole::SysSerial::Initialize()
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
SysConsole::SysSerial::Getc(u8 *c)
{
	u32 x = 0;
	if (im) {
		x = im->DisableIntr();
	}
	lock.Lock();
	if (inb(iobase + UART_LSR) & UART_DATA_READY) {
	    *c = inb(iobase + UART_RX);
	    lock.Unlock();
	    return IOS_OK;
	}
	lock.Unlock();
	if (im) {
		im->RestoreIntr(x);
	}
	return IOS_NODATA;
}

Device::IOStatus
SysConsole::SysSerial::Putc(u8 c)
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

DefineDevFactory(SysConsole::SysSerial);

RegDevClass(SysConsole::SysSerial, "sysser", Device::T_CHAR, "System serial interface");

/* System console device */

SysConsole::SysConsole(Type type, u32 unit, u32 classID) :
	ConsoleDev(type, unit, classID)
{
	defInput = defOutput = (ChrDevice *)devMan.CreateDevice("sysser");
	SetInputDevice(defInput);
	SetOutputDevice(defOutput);
	LIST_INIT(outClients);
	devState = S_UP;
}

SysConsole::~SysConsole()
{

}

Device::IOStatus
SysConsole::Putc(u8 c)
{
	Device::IOStatus rc = defOutput->Putc(c);
	outClientsMtx.Lock();
	OutputClient *oc;
	LIST_FOREACH(OutputClient, list, oc, outClients) {
		oc->dev->Putc(c);
	}
	outClientsMtx.Unlock();
	return rc;
}

int
SysConsole::RestoreDefInputDevice()
{
	return SetInputDevice(defInput);
}

int
SysConsole::AddOutputDevice(ChrDevice *dev)
{
	OutputClient *c = NEWSINGLE(OutputClient);
	if (!c) {
		return -1;
	}
	c->dev = dev;
	outClientsMtx.Lock();
	LIST_ADD(list, c, outClients);
	outClientsMtx.Unlock();
	return 0;
}

int
SysConsole::RemoveOutputDevice(ChrDevice *dev)
{
	outClientsMtx.Lock();
	OutputClient *c;
	LIST_FOREACH(OutputClient, list, c, outClients) {
		if (c->dev == dev) {
			LIST_DELETE(list, c, outClients);
			DELETE(c);
			outClientsMtx.Unlock();
			return 0;
		}
	}
	outClientsMtx.Unlock();
	return -1;
}

DefineDevFactory(SysConsole);

RegDevClass(SysConsole, "syscons", Device::T_CHAR, "System serial console");
