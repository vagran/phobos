/*
 * /kernel/kdb/debugger.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <kdb/debugger.h>

Debugger::Debugger *sysDebugger;

void
RunDebugger(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	printf("Debugger: ");
	vprintf(fmt, va);
	printf("\n");
	sysDebugger->Break();
}

void
_panic(const char *fileName, int line, const char *fmt,...)
{
	cli();
	va_list va;
	va_start(va, fmt);
	printf("panic: %s@%d: ", fileName, line);
	vprintf(fmt, va);
	printf("\n");
	if (Debugger::debugPanics) {
		if (sysDebugger) {
			sysDebugger->Break();
		}
	}
	hlt();
	while (1);
}

/*************************************************************/

Debugger::CmdDesc Debugger::cmds[] = {
	{"continue",	&Debugger::cmd_continue},
	{"c",			&Debugger::cmd_continue},
	{"registers",	&Debugger::cmd_registers},
	{"gdb",			&Debugger::cmd_gdb},
	{"step",		&Debugger::cmd_step},
	{"listthreads",	&Debugger::cmd_listthreads},
	{"thread",		&Debugger::cmd_thread},
};

int Debugger::debugFaults = 1;
int Debugger::debugPanics = 1;

Debugger::Debugger(ConsoleDev *con)
{
	gdbMode = 0;
	requestedBreak = 0;
	curCpu = 0;
	curThread = 0;
	reqRef = 0;
	reqValid = 0;
	numThreads = 0;
	LIST_INIT(threads);
	SetConsole(con);
	idt->RegisterHandler(IDT::ST_BREAKPOINT, (IDT::TrapHandler)_BPHandler, this);
	idt->RegisterHandler(IDT::ST_DEBUG, (IDT::TrapHandler)_DebugHandler, this);
	/* we use NMI for inter-processor debugging requests */
	idt->RegisterHandler(IDT::ST_NMI, (IDT::TrapHandler)_SMPDbgReqHandler, this);
}

int
Debugger::Break()
{
	requestedBreak = 1;
	__asm __volatile ("int $3");
	return 0;
}

Debugger::CmdDesc *
Debugger::FindCommand(char *s)
{
	for (u32 i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
		if (!strcmp(s, cmds[i].name)) {
			return &cmds[i];
		}
	}
	return 0;
}

int
Debugger::SetConsole(ConsoleDev *con)
{
	this->con = con;
	return 0;
}

int
Debugger::_BPHandler(Frame *frame, Debugger *d)
{
	return d->BPHandler(frame);
}

int
Debugger::_DebugHandler(Frame *frame, Debugger *d)
{
	d->requestedBreak = 1; /* prevent from eip decrementing */
	return d->BPHandler(frame);
}

int
Debugger::Trap(Frame *frame)
{
	con->Printf("Trap: %lu - %s\n", frame->vectorIdx, IDT::StrTrap((IDT::SysTraps)frame->vectorIdx));
	requestedBreak = 1;
	return BPHandler(frame);
}

Debugger::Thread *
Debugger::GetThread()
{
	threadLock.Lock();
	CPU *cpu = CPU::GetCurrent();
	if (!cpu) {
		threadLock.Unlock();
		return 0;
	}
	Thread *t;
	LIST_FOREACH(Thread, list, t, threads) {
		if (t->cpu == cpu) {
			threadLock.Unlock();
			return t;
		}
	}
	t = (Thread *)MM::malloc(sizeof(Thread));
	assert(t);
	memset(t, 0, sizeof(Thread));
	t->state = Thread::S_NONE;
	t->cpu = cpu;
	t->frame = 0;
	t->id = numThreads + 1;

	LIST_ADD(list, t, threads);
	numThreads++;
	threadLock.Unlock();
	return t;
}

Debugger::Thread *
Debugger::FindThread(u32 id)
{
	threadLock.Lock();
	Thread *t;
	LIST_FOREACH(Thread, list, t, threads) {
		if (t->id == id) {
			threadLock.Unlock();
			return t;
		}
	}
	threadLock.Unlock();
	return 0;
}

Debugger::Thread::State
Debugger::SetThreadState(Thread *thread, Thread::State state, Frame *frame)
{
	threadLock.Lock();
	Thread::State prevState = thread->state;
	thread->state = state;
	thread->frame = frame;
	threadLock.Unlock();
	return prevState;
}

int
Debugger::DRHandler(Frame *frame)
{
	u32 intr = IM::DisableIntr();
	while (reqLock.TryLock()) {
		if (!reqValid) {
			/* it's too late... */
			return 0;
		}
		pause();
	}
	AtomicOp::Inc(&reqRef);
	reqLock.Unlock();

	Thread *thread = GetThread();
	if (!thread) {
		panic("Debug request received without thread info");
	}
	switch (curReq) {
	case DRQ_STOP:
		AtomicOp::Dec(&reqRef);
		if (thread->state == Thread::S_STOPPED) {
			break;
		}
		SetThreadState(thread, Thread::S_STOPPED, frame);
		StopLoop(0);
		break;
	case DRQ_CONTINUE:
		AtomicOp::Dec(&reqRef);
		SetThreadState(thread, Thread::S_RUNNING, frame);
		break;
	default:
		AtomicOp::Dec(&reqRef);
	}
	if (thread == curThread) {
		curThread = 0;
		curCpu = 0;
	}
	IM::RestoreIntr(intr);
	return 0;
}

Debugger::HdlStatus
Debugger::StopLoop(int printStatus)
{
	Thread *thread = GetThread();
	while (1) {
		if (thread == curThread) {
			/* the thread is activated */
			RunLoop(thread->frame, printStatus);
			break;
		}
		if (thread->state != Thread::S_STOPPED) {
			break;
		}
		pause();
	}
	return HS_EXIT;
}

int
Debugger::SendDebugRequest(DebugRequest req, CPU *cpu)
{
	reqSendLock.Lock();
	reqLock.Lock();
	reqValid = 0;
	while (reqRef) {
		pause();
	}
	curReq = req;
	int rc = -1;
	while (1) {
		CPU *curCpu = CPU::GetCurrent();
		if (!curCpu) {
			reqLock.Unlock();
			break;
		}
		LAPIC *l = curCpu->GetLapic();
		if (!l) {
			reqLock.Unlock();
			break;
		}
		reqValid = 1;
		l->SendIPI(LAPIC::DM_NMI,
			cpu ? LAPIC::DST_SPECIFIC : LAPIC::DST_OTHERS,
			-1, 0, cpu ? cpu->GetID() : 0);
		reqLock.Unlock();
		l->WaitIPI();
		rc = 0;
		break;
	}
	reqSendLock.Unlock();
	return rc;
}

int
Debugger::_SMPDbgReqHandler(Frame *frame, Debugger *d)
{
	return d->DRHandler(frame);
}

int
Debugger::BPHandler(Frame *frame)
{
	u32 intr = IM::DisableIntr();
	CPU *cpu = CPU::GetCurrent();
	if (cpu && curCpu && cpu == curCpu) {
		debugPanics = 0;
		panic("Double fault in debugger code");
	}
	/*
	 * if break was initiated by dynamic breakpoint, eip
	 * must be decremented to point on current instruction.
	 */
	if (!requestedBreak) {
		frame->eip--;
	} else {
		requestedBreak = 0;
	}
	frame->eflags &= ~EFLAGS_TF;

	SendDebugRequest(DRQ_STOP); /* stop other CPUs */

	Thread *thread = GetThread();
	if (thread) {
		SetThreadState(thread, Thread::S_STOPPED, frame);
		if (curThread != thread) {
			SwitchThread(thread->id, 1);
			curCpu = 0;
			curThread = 0;
			IM::RestoreIntr(intr);
			return 0;
		}
	} else {
		if (curThread) {
			panic("Entered debugger in SMP mode without thread found");
		}
	}

	RunLoop(frame);
	curThread = 0;
	curCpu = 0;
	IM::RestoreIntr(intr);
	return 0;
}

int
Debugger::SetFrame(Frame *frame)
{
	this->frame = frame;
	/* esp is saved in frame only if PL was switched */
	if (!((SDT::SegSelector *)(void *)&frame->cs)->rpl) {
		__asm __volatile ("movw %%ss, %0" : "=g"(ss) : : );
		esp = (u32)&frame->esp;
	} else {
		esp = frame->esp;
		ss = frame->ss;
	}
	return 0;
}

int
Debugger::RunLoop(Frame *frame, int printStatus)
{
	Thread *thread = GetThread();
	if (thread) {
		thread->inRunLoop++;
	}

	SetFrame(frame);

	if (printStatus) {
		PrintFrameInfo(frame);
	}

	while (1) {
		if (!gdbMode) {
			Shell();
		}
		if (gdbMode) {
			GDB();
		} else {
			break;
		}
		if (gdbMode) {
			break;
		}
	}
	if (thread) {
		thread->inRunLoop--;
	}
	return 0;
}

int
Debugger::PrintFrameInfo(Frame *frame)
{
	if (gdbMode) {
		SendStatus(frame);
	} else {
		if (curCpu) {
			con->Printf("[CPU %lu-ID%lx] ", curCpu->GetUnit(), curCpu->GetID());
		} else {
			con->Printf("[Unknown CPU] ");
		}
		con->Printf("Stopped at 0x%08lx\n", frame->eip);
	}
	return 0;
}

void
Debugger::Prompt()
{
	con->Printf("PhobOS debugger [%08lx]> ", frame->eip);
}

int
Debugger::GetLine()
{
	lbSize = 0;
	while (1) {
		u8 c;
		while (con->Getc(&c) != Device::IOS_OK);
		switch (c) {
		case '\n':
		case '\r':
			con->Putc('\n');
			lineBuf[lbSize] = 0;
			return 0;
		case ConsoleDev::K_BACKSPACE:
			if (lbSize) {
				con->Putc(c);
				con->Putc(' ');
				con->Putc(c);
				lbSize--;
			}
			break;
		case ConsoleDev::K_UP:
		case ConsoleDev::K_DOWN:
		case ConsoleDev::K_RIGHT:
		case ConsoleDev::K_LEFT:
			break;
		default:
			if (lbSize >= sizeof(lineBuf) - 1) {
				break;
			}
			lineBuf[lbSize++] = c;
			con->Putc(c);
		}
	}
	return 0;
}

int
Debugger::GetHex(u8 d)
{
	if (d >= '0' && d <= '9') {
		return d - '0';
	}
	if (d >= 'a' && d <= 'f') {
		return d - 'a' + 10;
	}
	if (d >= 'A' && d <= 'F') {
		return d - 'A' + 10;
	}
	return -1;
}

int
Debugger::GetPacket()
{
	u32 i, csIdx = 0;
	int j;
	int state = 0;
	u8 cs = 0, bcs = 0;

	lbSize = 0;
	while (1) {
		u8 c;
		while (con->Getc(&c) != Device::IOS_OK) {
			pause();
		}
		switch (state) {
		case 0:
			/* wait packet start ($) */
			if (c != '$') {
				continue;
			}
			state++;
			bcs = 0;
			continue;
		case 1:
			/* receive packet data */
			switch (c) {
			case '#':
				/* end of packet */
				state++;
				csIdx = 0;
				continue;
			case '*':
				/* run-length encoding */
				if (!lbSize) {
					con->Putc('+');
					GDBSend("E00");
					continue;
				}
				bcs += c;
				state = 3;
				continue;
			case ':':
				if (lbSize == 2) {
					/* sequence ID, drop it */
					lbSize = 0;
					continue;
				}
				/* fall through */
			default:
				if (lbSize >= sizeof(lineBuf) - 1) {
					con->Putc('+');
					GDBSend("E00");
					state = 0;
					continue;
				}
				bcs += c;
				lineBuf[lbSize++] = c;
				continue;
			}
		case 2:
			/* checksum */
			j = GetHex(c);
			if (j < 0) {
				con->Putc('+');
				GDBSend("E00");
				state = 0;
				continue;
			}
			if (!csIdx) {
				cs = (j << 4) & 0xff;
				csIdx++;
				continue;
			}
			cs |= j & 0xf;
			if (cs != bcs) {
				con->Putc('-');
				state = 0;
				continue;
			}
			/* valid packet received */
			con->Putc('+');
			lineBuf[lbSize] = 0;
			return 0;
		case 3:
			/* run-length encoding */
			bcs += c;
			i = c + 29;
			if (lbSize + i >= sizeof(lineBuf) - 1) {
				con->Putc('+');
				GDBSend("E00");
				state = 0;
				continue;
			}
			c = lineBuf[lbSize];
			for (; i; i--) {
				lineBuf[lbSize++] = c;
			}
			state = 1;
			continue;
		}
	}
	return 0;
}

void
Debugger::GDBSend(const char *data, int reqAck)
{
	u8 cs;
	const char *s;
	static const char *hex = "0123456789abcdef";

	while (1) {
		con->Putc('$');
		cs = 0;
		for (s = data; *s; s++) {
			cs += (u8)*s;
			con->Putc(*s);
		}
		con->Putc('#');
		con->Putc(hex[cs >> 4]);
		con->Putc(hex[cs & 0xf]);
		if (reqAck) {
			u8 c;
			while (1) {
				while (con->Getc(&c) != Device::IOS_OK) {
					pause();
				}
				if (c == '+') {
					return;
				}
				if (c == '-') {
					/* retransmit */
					break;
				}
			}
		} else {
			return;
		}
	}
}

Debugger::HdlStatus
Debugger::cmd_continue(char **argv, u32 argc)
{
	return HS_EXIT;
}

Debugger::HdlStatus
Debugger::cmd_step(char **argv, u32 argc)
{
	frame->eflags |= EFLAGS_TF;
	return HS_EXIT;
}

Debugger::HdlStatus
Debugger::cmd_gdb(char **argv, u32 argc)
{
	con->Printf("Entering into remote GDB mode, type '+$D#44' to exit\n");
	gdbMode = 1;
	gdbSkipCont = 1; /* workaround for Eclipse GDB configuration */
	return HS_EXIT;
}

Debugger::HdlStatus
Debugger::cmd_listthreads(char **argv, u32 argc)
{
	threadLock.Lock();
	Thread *t;
	LIST_FOREACH(Thread, list, t, threads) {
		con->Printf("%c%lu. cpu %lu-ID%lu @ 0x%08lx\n",
			t->cpu == curCpu ? '>' : ' ',
			t->id,
			t->cpu->GetUnit(), t->cpu->GetID(),
			t->frame->eip);
	}
	threadLock.Unlock();
	return HS_OK;
}

Debugger::HdlStatus
Debugger::cmd_thread(char **argv, u32 argc)
{
	if (argc != 2) {
		con->Printf("Usage: thread <tread_id>\n");
		return HS_ERROR;
	}
	char *eptr;
	u32 id = strtoul(argv[1], &eptr, 10);
	if (*eptr) {
		con->Printf("Thread ID should be decimal integer ('%s' is invalid)\n", argv[1]);
		return HS_ERROR;
	}
	HdlStatus rc = SwitchThread(id, 1);
	if (rc == HS_ERROR) {
		con->Printf("Failed to switch to thread %lu\n", id);
		return HS_ERROR;
	}
	return rc;
}

Debugger::HdlStatus
Debugger::SwitchThread(u32 id, int printStatus)
{
	Thread *t = FindThread(id);
	if (!t) {
		return HS_ERROR;
	}
	if (gdbMode) {
		GDBSend("OK");
	}
	if (curThread == t) {
		return HS_OK;
	}
	curCpu = t->cpu;
	curThread = t;
	return StopLoop(printStatus);
}

int
Debugger::SetGDBMode(int f)
{
	int ret = gdbMode;
	gdbMode = f;
	gdbSkipCont = 1;
	return ret;
}

Debugger::HdlStatus
Debugger::cmd_registers(char **argv, u32 argc)
{
	con->Printf(
		"eax = 0x%08lx\n"
		"ebx = 0x%08lx\n"
		"ecx = 0x%08lx\n"
		"edx = 0x%08lx\n"
		"esi = 0x%08lx\n"
		"edi = 0x%08lx\n"
		"ebp = 0x%08lx\n"
		"esp = 0x%08lx\n"
		"eip = 0x%08lx\n"
		"eflags = 0x%08lx\n"
		"cs = 0x%08lx\n"
		"ds = 0x%08lx\n"
		"es = 0x%08lx\n"
		"fs = 0x%08lx\n"
		"gs = 0x%08lx\n"
		"ss = 0x%04hx\n",
		frame->eax,
		frame->ebx,
		frame->ecx,
		frame->edx,
		frame->esi,
		frame->edi,
		frame->ebp,
		esp,
		frame->eip,
		frame->eflags,
		(u32)frame->cs | ((u32)frame->__cs_pad << 16),
		(u32)frame->ds | ((u32)frame->__ds_pad << 16),
		(u32)frame->es | ((u32)frame->__es_pad << 16),
		(u32)frame->fs | ((u32)frame->__fs_pad << 16),
		(u32)frame->gs | ((u32)frame->__gs_pad << 16),
		ss
		);
	return HS_OK;
}

void
Debugger::ParseLine()
{
	char *s = lineBuf;
	argc = 0;
	while (*s) {
		while (*s == ' ' || *s == '\t') {
			s++;
		}
		if (!*s) {
			break;
		}
		argv[argc++] = s;
		char *next = strchr(s, ' ');
		if (!next) {
			next = strchr(s, '\t');
		}
		if (!next) {
			break;
		}
		*next = 0;
		s = next + 1;
	}
}

int
Debugger::Shell()
{
	while (1) {
		Prompt();
		GetLine();
		ParseLine();
		if (!argc) {
			continue;
		}
		CmdDesc *cmd = FindCommand(argv[0]);
		if (!cmd) {
			con->Printf("Command not found: '%s'\n", argv[0]);
			continue;
		}
		switch ((this->*(cmd->hdl))(argv, argc)) {
		case HS_EXIT:
			return 0;
		default:
			break;
		}
	}
	return 0;
}

int
Debugger::GDBThreadState()
{
	char *eptr;
	u32 id = strtoul(&lineBuf[1], &eptr, 10);
	if (*eptr) {
		GDBSend("E01");
		return 0;
	}
	if (numThreads) {
		if (FindThread(id)) {
			GDBSend("OK");
		} else {
			GDBSend("E00");
		}
	} else {
		/* no SMP yet, accept default thread only */
		if (id == 1) {
			GDBSend("OK");
		} else {
			GDBSend("E00");
		}
	}
	return 0;
}

int
Debugger::GDBSetThread()
{
	if (lineBuf[1] != 'c' && lineBuf[1] != 'g') {
		GDBSend("E00");
		return 0;
	}
	char *eptr;
	if (lineBuf[2] == '-' && lineBuf[3] == '1') {
		GDBSend("OK");
		return 0;
	}
	u32 id = strtoul(&lineBuf[2], &eptr, 10);
	if (*eptr) {
		GDBSend("E01");
		return 0;
	}
	if (!id || (!numThreads && id == 1)) {
		GDBSend("OK");
		return 0;
	}
	HdlStatus hs = SwitchThread(id);
	if (hs == HS_ERROR) {
		GDBSend("E02");
		return 0;
	}
	return hs == HS_EXIT ? 1 : 0;
}

int
Debugger::Query()
{
	char buf[256];

	if (!strcmp(lineBuf, "qC")) {
		/* query current thread ID */
		Thread *t = GetThread();
		sprintf(buf, "QC%lx", t ? t->id : 1);
		GDBSend(buf);
	} else if (!strcmp(lineBuf, "qfThreadInfo")) {
		/* return all threads in the first query */
		buf[0] = 'm';
		int pos = 1;
		threadLock.Lock();
		Thread *t;
		LIST_FOREACH(Thread, list, t, threads) {
			pos += sprintf(&buf[pos], "%lx", t->id);
			if (!LIST_ISLAST(list, t, threads)) {
				buf[pos++] = ',';
			}
		}
		if (pos == 1) {
			/* no SMP yet, return default thread */
			buf[pos++] = '1';
		}
		buf[pos] = 0;
		threadLock.Unlock();
		GDBSend(buf);
	} else if (!strcmp(lineBuf, "qsThreadInfo")) {
		GDBSend("l");
	} else if (!strncmp(lineBuf, "qThreadExtraInfo", sizeof("qThreadExtraInfo") - 1)) {
		int pos = sizeof("qThreadExtraInfo") - 1;
		if (lineBuf[pos++] != ',') {
			GDBSend("E00");
			return 0;
		}
		char *eptr;
		u32 id = strtoul(&lineBuf[pos], &eptr, 16);
		if (*eptr) {
			GDBSend("E01");
			return 0;
		}
		if (numThreads) {
			Thread *t = FindThread(id);
			if (!t) {
				GDBSend("E02");
				return 0;
			}
			sprintf(buf, "CPU %lu (ID %lu)", t->cpu->GetUnit(), t->cpu->GetID());
		} else {
			/* no SMP yet */
			strcpy(buf, "Bootstrap processor");
		}
		char _buf[256];
		char *pbuf = _buf;
		DumpData(&pbuf, sizeof(_buf), buf, strlen(buf));
		*pbuf = 0;
		GDBSend(_buf);
		return 0;
	} else {
		return -1;
	}
	return 0;
}

int
Debugger::ProcessPacket()
{
	switch (lineBuf[0]) {
	case 'g':
		return ReadRegisters();
	case 'G':
		if (!WriteRegisters()) {
			GDBSend("OK");
		} else {
			GDBSend("E00");
		}
		return 0;
	case 'm':
		return ReadMemory();
	case 'M':
		if (!WriteMemory()) {
			GDBSend("OK");
		} else {
			GDBSend("E00");
		}
		return 0;
	case '?':
		return SendStatus(frame);
	case 'c':
		return Continue();
	case 's':
		return Step();
	case 'q':
		return Query();
	case 'H':
		return GDBSetThread();
	case 'T':
		return GDBThreadState();
	}
	return -1;
}

int
Debugger::GDB()
{
	while (1) {
		GetPacket();
		if (lineBuf[0] == 'D' || lineBuf[0] == 'k') {
			/* detach */
			gdbMode = 0;
			break;
		}
		int rc = ProcessPacket();
		if (rc < 0) {
			GDBSend("");
		} else if (rc) {
			/* continue */
			break;
		}
	}
	return 0;
}

void
Debugger::GDBTrace(char *buf)
{
	while (*buf) {
		char outBuf[4096], *pbuf = outBuf;
		*(pbuf++) = 'O';
		buf += DumpData(&pbuf, sizeof(outBuf) - (pbuf - outBuf) - 1, buf, strlen(buf));
		*pbuf = 0;
		GDBSend(outBuf, 0);
	}
}

int
Debugger::Trace(const char *fmt,...)
{
	if (!gdbMode) {
		return -1;
	}
	va_list va;
	va_start(va, fmt);
	return VTrace(fmt, va);
}

int
Debugger::VTrace(const char *fmt, va_list va)
{
	if (!gdbMode) {
		return -1;
	}
	char buf[4096];
	vsnprintf(buf, sizeof(buf), fmt, va);
	GDBTrace(buf);
	return 0;
}

int
Debugger::DumpData(char **buf, u32 bufSize, const void *data, u32 size)
{
	const char *hex = "0123456789abcdef";
	int read = 0;
	while (size && bufSize >= 2) {
		u8 b = *(u8 *)data;
		read++;
		**buf = hex[b >> 4];
		(*buf)++;
		**buf = hex[b & 0xf];
		(*buf)++;
		data = ((u8 *)data) + 1;
		size--;
		bufSize -= 2;
	}
	return read;
}

u32
Debugger::FetchData(char **buf, void *data, u32 size)
{
	int count = 0;
	while (size) {
		if (!**buf) {
			return count;
		}
		u8 b = GetHex(**buf) << 4;
		(*buf)++;
		if (!**buf) {
			return count;
		}
		b |= GetHex(**buf) & 0xf;
		(*buf)++;
		count++;
		*(u8 *)data = b;
		data  = (u8 *)data + 1;
		size--;
	}
	return count;
}

int
Debugger::ReadRegisters()
{
	char buf[1024], *pbuf = buf;
	u8 null[2] = {0, 0};

	for (int r = 0; r < GR_MAX; r++) {
		switch (r) {
		case GR_EAX:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->eax, 4);
			break;
		case GR_EBX:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->ebx, 4);
			break;
		case GR_ECX:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->ecx, 4);
			break;
		case GR_EDX:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->edx, 4);
			break;
		case GR_ESI:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->esi, 4);
			break;
		case GR_EDI:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->edi, 4);
			break;
		case GR_EBP:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->ebp, 4);
			break;
		case GR_ESP:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &esp, 4);
			break;
		case GR_EIP:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->eip, 4);
			break;
		case GR_EFLAGS:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->eflags, 4);
			break;
		case GR_CS:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->cs, 4);
			break;
		case GR_SS:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &ss, 2);
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), null, 2);
			break;
		case GR_DS:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->ds, 4);
			break;
		case GR_ES:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->es, 4);
			break;
		case GR_FS:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->fs, 4);
			break;
		case GR_GS:
			DumpData(&pbuf, sizeof(buf) - (pbuf - buf), &frame->gs, 4);
			break;
		}
	}
	*pbuf = 0;
	GDBSend(buf);
	return 0;
}

int
Debugger::WriteRegisters()
{
	char *pbuf = &lineBuf[1];
	u32 dummy;

	for (int r = 0; r < GR_MAX; r++) {
		switch (r) {
		case GR_EAX:
			if (FetchData(&pbuf, &frame->eax, 4) < 4) return 0;
			break;
		case GR_EBX:
			if (FetchData(&pbuf, &frame->ebx, 4) < 4) return 0;
			break;
		case GR_ECX:
			if (FetchData(&pbuf, &frame->ecx, 4) < 4) return 0;
			break;
		case GR_EDX:
			if (FetchData(&pbuf, &frame->edx, 4) < 4) return 0;
			break;
		case GR_ESI:
			if (FetchData(&pbuf, &frame->esi, 4) < 4) return 0;
			break;
		case GR_EDI:
			if (FetchData(&pbuf, &frame->edi, 4) < 4) return 0;
			break;
		case GR_EBP:
			if (FetchData(&pbuf, &frame->ebp, 4) < 4) return 0;
			break;
		case GR_ESP:
			/* we don't support ESP modification */
			if (FetchData(&pbuf, &dummy, 4) < 4) return 0;
			break;
		case GR_EIP:
			if (FetchData(&pbuf, &frame->eip, 4) < 4) return 0;
			break;
		case GR_EFLAGS:
			if (FetchData(&pbuf, &frame->eflags, 4) < 4) return 0;
			break;
		case GR_CS:
			if (FetchData(&pbuf, &frame->cs, 4) < 4) return 0;
			break;
		case GR_SS:
			/* we don't support SS modification */
			if (FetchData(&pbuf, &dummy, 4) < 4) return 0;
			break;
		case GR_DS:
			if (FetchData(&pbuf, &frame->ds, 4) < 4) return 0;
			break;
		case GR_ES:
			if (FetchData(&pbuf, &frame->es, 4) < 4) return 0;
			break;
		case GR_FS:
			if (FetchData(&pbuf, &frame->fs, 4) < 4) return 0;
			break;
		case GR_GS:
			if (FetchData(&pbuf, &frame->gs, 4) < 4) return 0;
			break;
		}
	}
	return 0;
}

int
Debugger::ReadMemory()
{
	char buf[8196], *pbuf = buf; /* 4KB hex-encoded data */
	u32 addr, size;
	char *eptr;

	addr = strtoul(&lineBuf[1], &eptr, 16);
	if (*eptr != ',') {
		GDBSend("E01");
		return 0;
	}

	size = strtoul(eptr + 1, &eptr, 16);
	while (size) {
		u32 toRead = roundup2(addr + 1, PAGE_SIZE) - addr;
		toRead = min(toRead, size);
		PTE::PDEntry *pde = MM::VtoPDE(addr);
		if (!pde->fields.present) {
			//Trace("Error: address not available (PDE): 0x%08lx\n", addr);
			GDBSend("E02");
			return 0;
		}
		PTE::PTEntry *pte = MM::VtoPTE(addr);
		if (!pte->fields.present) {
			//Trace("Error: address not available (PTE): 0x%08lx\n", addr);
			GDBSend("E03");
			return 0;
		}
		DumpData(&pbuf, sizeof(buf) - (pbuf - buf), (void *)addr, toRead);
		size -= toRead;
		addr += toRead;
	}
	*pbuf = 0;
	GDBSend(buf);
	return 0;
}

int
Debugger::WriteMemory()
{
	u32 addr, size;
	char *eptr;
	addr = strtoul(&lineBuf[1], &eptr, 16);
	if (*eptr != ',') {
		GDBSend("E01");
		return -1;
	}
	size = strtoul(eptr + 1, &eptr, 16);
	if (*eptr != ':') {
		GDBSend("E01");
		return -1;
	}
	eptr++;

	while (size) {
		u32 toWrite = roundup2(addr + 1, PAGE_SIZE) - addr;
		toWrite = min(toWrite, size);
		PTE::PDEntry *pde = MM::VtoPDE(addr);
		if (!pde->fields.present) {
			break;
		}
		PTE::PTEntry *pte = MM::VtoPTE(addr);
		if (!pte->fields.present) {
			break;
		}
		if (FetchData(&eptr, (void *)addr, toWrite) < toWrite) {
			break;
		}
		size -= toWrite;
		addr += toWrite;
	}
	return 0;
}

int
Debugger::SendStatus(Frame *frame)
{
	char buf[128];
	u32 esp;
	u16 ss;

	/* esp is saved in frame only if PL was switched */
	if (!((SDT::SegSelector *)(void *)&frame->cs)->rpl) {
		__asm __volatile ("movw %%ss, %0" : "=g"(ss) : : );
		esp = (u32)&frame->esp;
	} else {
		esp = frame->esp;
		ss = frame->ss;
	}
	sprintf(buf, "T05%x:%02x%02x%02x%02x;%x:%02x%02x%02x%02x;%x:%02x%02x%02x%02x;",
		GR_ESP, ((u8 *)&esp)[0], ((u8 *)&esp)[1], ((u8 *)&esp)[2], ((u8 *)&esp)[3],
		GR_EBP, ((u8 *)&frame->ebp)[0], ((u8 *)&frame->ebp)[1],
		((u8 *)&frame->ebp)[2], ((u8 *)&frame->ebp)[3],
		GR_EIP, ((u8 *)&frame->eip)[0], ((u8 *)&frame->eip)[1],
		((u8 *)&frame->eip)[2], ((u8 *)&frame->eip)[3]);
	GDBSend(buf);
	return 0;
}

int
Debugger::Continue()
{
	u32 addr;
	char *eptr;

	if (lineBuf[1]) {
		addr = strtoul(&lineBuf[1], &eptr, 16);
		PTE::PDEntry *pde = MM::VtoPDE(addr);
		if (!pde->fields.present) {
			GDBSend("E00");
			return 0;
		}
		PTE::PTEntry *pte = MM::VtoPTE(addr);
		if (!pte->fields.present) {
			GDBSend("E00");
			return 0;
		}
		frame->eip = addr;
	}

	if (gdbSkipCont) {
		SendStatus(frame);
		gdbSkipCont--;
		return 0;
	}
	return 1;
}

int
Debugger::Step()
{
	u32 addr;
	char *eptr;

	if (lineBuf[1]) {
		addr = strtoul(&lineBuf[1], &eptr, 16);
		PTE::PDEntry *pde = MM::VtoPDE(addr);
		if (!pde->fields.present) {
			GDBSend("E00");
			return 0;
		}
		PTE::PTEntry *pte = MM::VtoPTE(addr);
		if (!pte->fields.present) {
			GDBSend("E00");
			return 0;
		}
		frame->eip = addr;
	}
	frame->eflags |= EFLAGS_TF;
	return 1;
}
