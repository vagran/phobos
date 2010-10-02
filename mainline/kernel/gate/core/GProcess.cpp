/*
 * /phobos/kernel/gate/GProcess.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

DEFINE_GCLASS(GProcess);

GProcess::GProcess()
{

}

GProcess::~GProcess()
{

}

PM::pid_t
GProcess::GetPID()
{
	return proc->GetID();
}

PM::pid_t
GProcess::GetThreadID()
{
	return PM::Thread::GetCurrent()->GetID();
}

int
GProcess::GetName(char *buf, int bufLen)
{
	if (buf) {
		if (proc->CheckUserBuf(buf, bufLen, MM::PROT_WRITE)) {
			return -1;
		}
	}
	return proc->GetName()->Get(buf, bufLen);
}
