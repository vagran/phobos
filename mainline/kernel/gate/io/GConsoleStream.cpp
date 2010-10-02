/*
 * /phobos/kernel/gate/io/GConsoleStream.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

DEFINE_GCLASS(GConsoleStream);

GConsoleStream::GConsoleStream(const char *name, ConsoleDev *con, int flags) :
	GStream(name)
{
	con->AddRef();
	this->con = con;
	this->flags = flags;
}

GConsoleStream::~GConsoleStream()
{
	con->Release();
}

int
GConsoleStream::Read(u8 *buf, u32 size)
{
	if (!(flags & F_INPUT)) {
		return E_NOTSUPPORTED;
	}
	if (proc->CheckUserBuf(buf, size, MM::PROT_WRITE)) {
		return E_FAULT;
	}
	int rc = con->Read(buf, size);
	if (rc < 0) {
		if (rc == -Device::IOS_NOTSPRT) {
			return E_NOTSUPPORTED;
		}
		return E_FAULT;
	}
	return rc;
}

int
GConsoleStream::Write(u8 *buf, u32 size)
{
	if (!(flags & F_OUTPUT)) {
		return E_NOTSUPPORTED;
	}
	if (proc->CheckUserBuf(buf, size, MM::PROT_READ)) {
		return E_FAULT;
	}
	int rc = con->Write(buf, size);
	if (rc < 0) {
		if (rc == -Device::IOS_NOTSPRT) {
			return E_NOTSUPPORTED;
		}
		return E_FAULT;
	}
	return rc;
}
