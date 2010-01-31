/*
 * /kernel/dev/uart/ser.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef SER_H_
#define SER_H_
#include <sys.h>
phbSource("$Id$");

class SerialPort : public ChrDevice {
public:
	enum UART {
		/* The offsets of UART registers.  */
		UART_TX =		0,
		UART_RX =		0,
		UART_DLL =		0,
		UART_IER =		1,
		UART_DLH =		1,
		UART_IIR =		2,
		UART_FCR =		2,
		UART_LCR =		3,
		UART_MCR =		4,
		UART_LSR =		5,
		UART_MSR =		6,
		UART_SR =		7,

		/* For LSR bits.  */
		UART_DATA_READY =			0x01,
		UART_EMPTY_TRANSMITTER =	0x20,

		/* The type of parity.  */
		UART_NO_PARITY =			0x00,
		UART_ODD_PARITY =			0x08,
		UART_EVEN_PARITY =			0x18,

		/* The type of word length.  */
		UART_5BITS_WORD =			0x00,
		UART_6BITS_WORD =			0x01,
		UART_7BITS_WORD =			0x02,
		UART_8BITS_WORD =			0x03,

		/* The type of the length of stop bit.  */
		UART_1_STOP_BIT =			0x00,
		UART_2_STOP_BITS =			0x04,

		/* the switch of DLAB.  */
		UART_DLAB =					0x80,

		/* Enable the FIFO.  */
		UART_ENABLE_FIFO =			0xC7,

		/* Turn on DTR, RTS, and OUT2.  */
		UART_ENABLE_MODEM =			0x0B,
	};
private:
	enum {
		BASE_SPEED =		115200,
		DEFAULT_SPEED =		BASE_SPEED,
	};

	u16 iobase;
	u16 divisor;
	SpinLock lock;

	void Initialize();
	void SetSpeed(int speed);
public:
	DeclareDevFactory();
	SerialPort(Type type, u32 unit, u32 classID);
	virtual IOStatus Getc(u8 *c);
	virtual IOStatus Putc(u8 c);
};

#endif /* SER_H_ */
