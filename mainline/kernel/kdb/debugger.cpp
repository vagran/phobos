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

/*************************************************************/

Debugger::CmdDesc Debugger::cmds[] = {
	{"continue", &Debugger::cmd_continue},
	{"c", &Debugger::cmd_continue},
	{"registers", &Debugger::cmd_registers},
	{"gdb", &Debugger::cmd_gdb},
	{"step", &Debugger::cmd_step},
};

Debugger::Debugger(ConsoleDev *con)
{
	gdbMode = 0;
	requestedBreak = 0;
	SetConsole(con);
	idt->RegisterHandler(IDT::ST_BREAKPOINT, (IDT::TrapHandler)_BPHandler, this);
	idt->RegisterHandler(IDT::ST_DEBUG, (IDT::TrapHandler)_DebugHandler, this);
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
Debugger::_BPHandler(u32 idx, Debugger *d, Frame *frame)
{
	return d->BPHandler(frame);
}

int
Debugger::_DebugHandler(u32 idx, Debugger *d, Frame *frame)
{
	d->requestedBreak = 1; /* prevent from eip decrementing */
	return d->BPHandler(frame);
}

int
Debugger::BPHandler(Frame *frame)
{
	this->frame = frame;
	PrintFrameInfo(frame);
	/* esp is saved in frame only if PL was switched */
	if (!((SDT::SegSelector *)&frame->cs)->rpl) {
		__asm __volatile ("movw %%ss, %0" : "=g"(ss) : : );
		esp = (u32)&frame->esp;
	} else {
		esp = frame->esp;
		ss = frame->ss;
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
	return 0;
}

int
Debugger::PrintFrameInfo(Frame *frame)
{
	con->Printf("Stopped at 0x%08x\n", frame->eip);
	return 0;
}

void
Debugger::Prompt()
{
	con->Printf("PhobOS debugger [%08x]> ", frame->eip);
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
	u32 i, csIdx;
	int j;
	int state = 0;
	u8 cs, bcs;

	lbSize = 0;
	while (1) {
		u8 c;
		while (con->Getc(&c) != Device::IOS_OK);
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
Debugger::GDBSend(const char *data)
{
	u8 cs;
	const char *s;
	static const char *hex = "0123456789ABCDEF";

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
		u8 c;
		while (1) {
			while (con->Getc(&c) != Device::IOS_OK);
			if (c == '+') {
				return;
			}
			if (c == '-') {
				/* retransmit */
				break;
			}
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
Debugger::cmd_registers(char **argv, u32 argc)
{
	con->Printf(
		"eax = 0x%08x\n"
		"ebx = 0x%08x\n"
		"ecx = 0x%08x\n"
		"edx = 0x%08x\n"
		"esi = 0x%08x\n"
		"edi = 0x%08x\n"
		"ebp = 0x%08x\n"
		"esp = 0x%08x\n"
		"eip = 0x%08x\n"
		"eflags = 0x%08x\n"
		"cs = 0x%08x\n"
		"ds = 0x%08x\n"
		"es = 0x%08x\n"
		"fs = 0x%08x\n"
		"gs = 0x%08x\n"
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
		s = next;
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
	case 'H':
		GDBSend("OK");
		return 0;
	case '?':
		return SendStatus();
	case 'c':
		return Continue();
	case 's':
		return Step();
	}
	return -1;
}

int
Debugger::GDB()
{
	SendStatus();
	while (1) {
		GetPacket();
		if (!strcmp("D", lineBuf)) {
			/* detach */
			gdbMode = 0;
			return 0;
		}
		int rc = ProcessPacket();
		if (rc < 0) {
			GDBSend("");
		} else if (rc) {
			/* continue */
			return 0;
		}
	}
	return 0;
}

int
Debugger::DumpData(char **buf, void *data, u32 size)
{
	const char *hex = "0123456789ABCDEF";
	while (size) {
		u8 b = *(u8 *)data;
		**buf = hex[b >> 4];
		(*buf)++;
		**buf = hex[b & 0xf];
		(*buf)++;
		data = ((u8 *)data) + 1;
		size--;
	}
	return 0;
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
			DumpData(&pbuf, &frame->eax, 4);
			break;
		case GR_EBX:
			DumpData(&pbuf, &frame->ebx, 4);
			break;
		case GR_ECX:
			DumpData(&pbuf, &frame->ecx, 4);
			break;
		case GR_EDX:
			DumpData(&pbuf, &frame->edx, 4);
			break;
		case GR_ESI:
			DumpData(&pbuf, &frame->esi, 4);
			break;
		case GR_EDI:
			DumpData(&pbuf, &frame->edi, 4);
			break;
		case GR_EBP:
			DumpData(&pbuf, &frame->ebp, 4);
			break;
		case GR_ESP:
			DumpData(&pbuf, &esp, 4);
			break;
		case GR_EIP:
			DumpData(&pbuf, &frame->eip, 4);
			break;
		case GR_EFLAGS:
			DumpData(&pbuf, &frame->eflags, 4);
			break;
		case GR_CS:
			DumpData(&pbuf, &frame->cs, 4);
			break;
		case GR_SS:
			DumpData(&pbuf, &ss, 2);
			DumpData(&pbuf, null, 2);
			break;
		case GR_DS:
			DumpData(&pbuf, &frame->ds, 4);
			break;
		case GR_ES:
			DumpData(&pbuf, &frame->es, 4);
			break;
		case GR_FS:
			DumpData(&pbuf, &frame->fs, 4);
			break;
		case GR_GS:
			DumpData(&pbuf, &frame->gs, 4);
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
		return -1;
	}
	size = strtoul(eptr + 1, &eptr, 16);
	while (size) {
		u32 toRead = roundup2(addr + 1, PAGE_SIZE) - addr;
		toRead = min(toRead, size);
		PTE::PDEntry *pde = mm::VtoPDE(addr);
		if (!pde->fields.present) {
			break;
		}
		PTE::PTEntry *pte = mm::VtoPTE(addr);
		if (!pte->fields.present) {
			break;
		}
		DumpData(&pbuf, (void *)addr, toRead);
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
		PTE::PDEntry *pde = mm::VtoPDE(addr);
		if (!pde->fields.present) {
			break;
		}
		PTE::PTEntry *pte = mm::VtoPTE(addr);
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
Debugger::SendStatus()
{
	char buf[128];

	sprintf(buf, "T05%X:%02X%02X%02X%02X;%X:%02X%02X%02X%02X;%X:%02X%02X%02X%02X;",
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
		PTE::PDEntry *pde = mm::VtoPDE(addr);
		if (!pde->fields.present) {
			GDBSend("E00");
			return 0;
		}
		PTE::PTEntry *pte = mm::VtoPTE(addr);
		if (!pte->fields.present) {
			GDBSend("E00");
			return 0;
		}
		frame->eip = addr;
	}

	if (gdbSkipCont) {
		SendStatus();
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
		PTE::PDEntry *pde = mm::VtoPDE(addr);
		if (!pde->fields.present) {
			GDBSend("E00");
			return 0;
		}
		PTE::PTEntry *pte = mm::VtoPTE(addr);
		if (!pte->fields.present) {
			GDBSend("E00");
			return 0;
		}
		frame->eip = addr;
	}
	frame->eflags |= EFLAGS_TF;
	return 1;
}
