/*
 * /phobos/kernel/gate/io/GConsoleStream.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef GCONSOLESTREAM_H_
#define GCONSOLESTREAM_H_
#include <sys.h>
phbSource("$Id$");

#include <dev/condev.h>

DECLARE_GCLASS(GConsoleStream);

class GConsoleStream : public GStream {
public:
	enum Flags {
		F_INPUT =	0x1,
		F_OUTPUT =	0x2,
	};

private:
	int flags;
	ConsoleDev *con;

	virtual waitid_t GetWaitChannel(Operation op);
	virtual int OpAvailable(Operation op);
public:
	GConsoleStream(const char *name, ConsoleDev *con, int flags = F_INPUT | F_OUTPUT);
	virtual ~GConsoleStream();

	/* return number of bytes read, EOF if end of stream, another negative value if error */
	virtual int Read(u8 *buf, u32 size);
	/* return number of bytes written, negative value if error */
	virtual int Write(u8 *buf, u32 size);

	DECLARE_GCLASS_IMP(GConsoleStream);
};

#endif /* GCONSOLESTREAM_H_ */
