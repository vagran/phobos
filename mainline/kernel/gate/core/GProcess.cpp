/*
 * /phobos/kernel/gate/GProcess.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

DEFINE_GCLASS(GThread);

GThread::GThread(PM::Thread *refThrd)
{
	if (refThrd) {
		this->refThrd = refThrd;
	} else {
		this->refThrd = PM::Thread::GetCurrent();
	}
}

GThread::~GThread()
{

}

PM::pid_t
GThread::GetPID()
{
	return refThrd->GetID();
}

PM::Thread::State
GThread::GetState(u32 *pExitCode, PM::ProcessFault *pFault)
{
	if (pExitCode && proc->CheckUserBuf(pExitCode, sizeof(*pExitCode),
		MM::PROT_WRITE)) {
		return PM::Thread::S_NONE;
	}
	if (pFault && proc->CheckUserBuf(pFault, sizeof(*pFault),
		MM::PROT_WRITE)) {
		return PM::Thread::S_NONE;
	}
	return refThrd->GetState(pExitCode, pFault);
}

PM::waitid_t
GThread::GetWaitChannel(Operation op)
{
	if (op != OP_READ) {
		return 0;
	}
	lastState = refThrd->GetState();
	return (PM::waitid_t)refThrd;
}

int
GThread::OpAvailable(Operation op)
{
	if (op != OP_READ) {
		return 0;
	}
	PM::Thread::State state = refThrd->GetState();
	/* Check if state has changed since last wait call */
	return (state == PM::Thread::S_TERMINATED) || (state != lastState);
}

/******************************************************************************/

DEFINE_GCLASS(GProcess);

GProcess::GProcess(PM::Process *refProc)
{
	if (refProc) {
		this->refProc = refProc;
	} else {
		this->refProc = proc;
	}
}

GProcess::~GProcess()
{

}

PM::pid_t
GProcess::GetPID()
{
	return refProc->GetID();
}

DEF_STR_PROV(GProcess::GetName)
{
	if (buf && proc->CheckUserBuf(buf, bufLen, MM::PROT_WRITE)) {
		return -1;
	}
	return refProc->GetName()->Get(buf, bufLen);
}

DEF_STR_PROV(GProcess::GetArgs)
{
	if (buf && proc->CheckUserBuf(buf, bufLen, MM::PROT_WRITE)) {
		return -1;
	}
	return refProc->GetArgs()->Get(buf, bufLen);
}

int
GProcess::SetArgs(const char *args)
{
	if (proc->CheckUserString(args)) {
		return -1;
	}
	*(refProc->GetArgs()) = args;
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
	//notimpl
	return 0;
}

PM::Process::State
GProcess::GetState(u32 *pExitCode, PM::ProcessFault *pFault)
{
	if (pExitCode && proc->CheckUserBuf(pExitCode, sizeof(*pExitCode),
		MM::PROT_WRITE)) {
		return PM::Process::S_NONE;
	}
	if (pFault && proc->CheckUserBuf(pFault, sizeof(*pFault),
		MM::PROT_WRITE)) {
		return PM::Process::S_NONE;
	}
	return refProc->GetState(pExitCode, pFault);
}

PM::waitid_t
GProcess::GetWaitChannel(Operation op)
{
	if (op != OP_READ) {
		return 0;
	}
	lastState = refProc->GetState();
	return (PM::waitid_t)refProc;
}

int
GProcess::OpAvailable(Operation op)
{
	if (op != OP_READ) {
		return 0;
	}
	PM::Process::State state = refProc->GetState();
	/* Check if state has changed since last wait call */
	return (state == PM::Process::S_TERMINATED) || (state != lastState);
}

GThread *
GProcess::CreateThread(GThread::ThreadFunc func, void *arg, u32 stackSize,
	u32 priority)
{
	PM::Thread *thrd = refProc->CreateUserThread((vaddr_t)func, stackSize,
		priority, 1, arg);
	if (!thrd) {
		return 0;
	}
	thrd->Run();
	GThread *gthrd = GNEW(gateArea, GThread, thrd);
	if (!gthrd) {
		refProc->TerminateThread(thrd, -1);
		ERROR(E_NOMEM, "Failed to create GThread object");
		return 0;
	}
	gthrd->AddRef();
	return gthrd;
}
