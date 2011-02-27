/*
 * /phobos/kernel/gate/GateStream.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

DEFINE_GCLASS(GStream);

GStream::GStream(const char *name)
{
	streamName = strdup(name);
	proc->AddStream(this);
}

GStream::~GStream()
{
	proc->RemoveStream(this);
	MM::mfree(streamName);
}

int
GStream::GetName(char *buf, int bufSize)
{
	if (!buf) {
		return strlen(streamName);
	}
	if (proc->CheckUserBuf(buf, bufSize, MM::PROT_WRITE)) {
		return -1;
	}
	strncpy(buf, streamName, bufSize);
	return strlen(buf);
}
