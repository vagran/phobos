/*
 * /phobos/kernel/gate/GateStream.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

DEFINE_GCLASS(GStream);

GStream::GStream(char *name)
{
	this->streamName = strdup(name);
}

GStream::~GStream()
{
	MM::mfree(streamName);
}

int
GStream::GetName(char *buf, int bufSize)
{
	if (!buf) {
		return strlen(streamName);
	}
	strncpy(buf, streamName, bufSize);
	return strlen(buf);
}
