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

DEF_STR_PROV(GProcess::GetName)
{
	if (buf && proc->CheckUserBuf(buf, bufLen, MM::PROT_WRITE)) {
		return -1;
	}
	return proc->GetName()->Get(buf, bufLen);
}

DEF_STR_PROV(GProcess::GetArgs)
{
	if (buf && proc->CheckUserBuf(buf, bufLen, MM::PROT_WRITE)) {
		return -1;
	}
	return proc->GetArgs()->Get(buf, bufLen);
}

int
GProcess::SetArgs(const char *args)
{
	if (proc->CheckUserString(args)) {
		return -1;
	}
	*(proc->GetArgs()) = args;
	return 0;
}

DEF_STR_PROV(GProcess::GetEnv)
{
	if (buf && proc->CheckUserBuf(buf, bufLen, MM::PROT_WRITE)) {
		return -1;
	}
	//notimpl
	return 0;
}

int
GProcess::SetEnv(const char *name, char *value)
{

	return 0;
}
