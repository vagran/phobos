/*
 * /kernel/dev/condev.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/condev.h>

ConsoleDev::ConsoleDev(Type type, u32 unit, u32 classID) :
	ChrDevice(type, unit, classID)
{
	inDev = 0;
	LIST_INIT(outClients);
	fgCol = COL_WHITE | COL_BRIGHT;
	bgCol = COL_BLACK;
	tabSize = 8;
	memset(&outQueue, 0, sizeof(outQueue));
	outIrq = 0;
}

ConsoleDev::~ConsoleDev()
{
	if (inDev) {
		inDev->Release();
	}
	outClientsMtx.Lock();
	OutputClient *oc;
	LIST_FOREACH(OutputClient, list, oc, outClients) {
		oc->dev->Release();
	}
	LIST_INIT(outClients);
	outClientsMtx.Unlock();
}

/* queued data are dropped after queue reconfiguration */
int
ConsoleDev::SetQueue(u32 inputQueueSize, u32 outputQueueSize)
{
	/* XXX input queue is not supported yet */

	if (outputQueueSize == outQueue.size) {
		return 0;
	}
	u64 x = im->SetPL(IM::IP_CONSOLE);
	outQueue.lock.Lock();
	if (outQueue.size) {
		MM::mfree(outQueue.data);
		outQueue.data = 0;
		if (!outputQueueSize) {
			im->ReleaseIrq(outIrq);
			outIrq = 0;
			outQueue.size = 0;
			outQueue.lock.Unlock();
			im->RestorePL(x);
			return 0;
		}
	} else {
		outIrq = im->AllocateSwirq(OutputIntr, this, 0,
			IM::AF_EXCLUSIVE, IM::IP_CONSOLE);
		ensure(outIrq);
	}
	outQueue.data = (u8 *)MM::malloc(outputQueueSize);
	ensure(outQueue.data);
	outQueue.size = outputQueueSize;
	outQueue.dataSize = 0;
	outQueue.pRead = 0;
	outQueue.pWrite = 0;
	outQueue.lock.Unlock();
	im->RestorePL(x);
	return 0;
}

IM::IsrStatus
ConsoleDev::OutputIntr(Handle h, void *arg)
{
	return ((ConsoleDev *)arg)->OutputIntr();
}

IM::IsrStatus
ConsoleDev::OutputIntr()
{
	/* probably some data queued, check them and output if present */
	outQueue.lock.Lock();
	while (outQueue.dataSize) {
		DevPutc(outQueue.data[outQueue.pRead]);
		if (++outQueue.pRead >= outQueue.size) {
			outQueue.pRead = 0;
		}
		outQueue.dataSize--;
	}
	outQueue.lock.Unlock();
	return IM::IS_PROCESSED;
}

/* output queue must be locked */
Device::IOStatus
ConsoleDev::QPutc(u8 c)
{
	u64 x = im->SetPL(IM::IP_CONSOLE);
	/* disable interrupts to avoid recursive locking */
	u32 intr = im->DisableIntr();
	outQueue.lock.Lock();
	IOStatus rc = IOS_ERROR;
	while (1) {
		if (outQueue.pWrite == outQueue.pRead && outQueue.dataSize) {
			/* queue full */
			break;
		}
		outQueue.data[outQueue.pWrite] = c;
		if (++outQueue.pWrite >= outQueue.size) {
			outQueue.pWrite = 0;
		}
		outQueue.dataSize++;
		rc = IOS_OK;
		break;
	}
	outQueue.lock.Unlock();
	im->RestoreIntr(intr);
	im->RestorePL(x);
	if (rc == IOS_OK) {
		im->Irq(outIrq);
	}
	return rc;
}

Device::IOStatus
ConsoleDev::DevPutc(u8 c)
{
	Device::IOStatus rc = IOS_OK;
	outClientsMtx.Lock();
	if (LIST_ISEMPTY(outClients)) {
		outClientsMtx.Unlock();
		return IOS_NOTSPRT;
	}
	OutputClient *oc;
	LIST_FOREACH(OutputClient, list, oc, outClients) {
		Device::IOStatus rc1 = oc->dev->Putc(c);
		if (rc1 != IOS_OK) {
			rc = rc1;
		}
	}
	outClientsMtx.Unlock();
	return rc;
}

Device::IOStatus
ConsoleDev::Putc(u8 c)
{
	if (!outQueue.size) {
		return DevPutc(c);
	}
	return QPutc(c);
}

Device::IOStatus
ConsoleDev::Getc(u8 *c)
{
	if (!inDev) {
		return IOS_NOTSPRT;
	}
	return inDev->Getc(c);
}

int
ConsoleDev::SetFgColor(int col)
{
	int r = fgCol;
	fgCol = col;
	return r;
}

int
ConsoleDev::SetBgColor(int col)
{
	int r = bgCol;
	bgCol = col;
	return r;
}

int
ConsoleDev::SetTabSize(int sz)
{
	int r = tabSize;
	tabSize = sz;
	return r;
}

int
ConsoleDev::Clear()
{
	/* could be implemented in derived classes */
	return 0;
}

int
ConsoleDev::Printf(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	return VPrintf(fmt, va);
}

int
ConsoleDev::VPrintf(const char *fmt, va_list va)
{
	return kvprintf(fmt, (PutcFunc)_Putc, this, 10, va);
}

void
ConsoleDev::_Putc(int c, ConsoleDev *p)
{
	p->Putc((u8)c);
}

int
ConsoleDev::SetInputDevice(ChrDevice *dev)
{
	ChrDevice *oldDev = inDev;
	dev->AddRef();
	inDev = dev;
	if (oldDev) {
		oldDev->Release();
	}
	return 0;
}

int
ConsoleDev::AddOutputDevice(ChrDevice *dev)
{
	OutputClient *c = NEWSINGLE(OutputClient);
	if (!c) {
		return -1;
	}
	dev->AddRef();
	c->dev = dev;
	outClientsMtx.Lock();
	LIST_ADD(list, c, outClients);
	outClientsMtx.Unlock();
	return 0;
}

int
ConsoleDev::RemoveOutputDevice(ChrDevice *dev)
{
	outClientsMtx.Lock();
	OutputClient *c;
	LIST_FOREACH(OutputClient, list, c, outClients) {
		if (c->dev == dev) {
			LIST_DELETE(list, c, outClients);
			dev->Release();
			DELETE(c);
			outClientsMtx.Unlock();
			return 0;
		}
	}
	outClientsMtx.Unlock();
	return -1;
}
