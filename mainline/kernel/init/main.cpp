/*
 * /kernel/init/main.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <boot.h>
#include <gcc.h>
#include <kdb/debugger.h>

MBInfo *pMBInfo;

char *argv[MAX_CMDLINE_PARAMS + 1];
int argc;

static int bootDebugger = 0;

static char copyright[]	=
	"******************************************************\n"
	"PhobOS operating system\n"
	"Written by Artemy Lebedev\n"
	"Copyright (c)AST 2009\n";

static int
InitTables()
{
	idt = NEWSINGLE(IDT);
	gdt = NEWSINGLE(GDT);
	return 0;
}

typedef enum {
	OPT_DEBUGGER,
	OPT_GDB,
} OptID;

typedef struct {
	const char *name;
	int hasArg;
	OptID id;
} Option;

static Option opts[] = {
	{"debugger", 0, OPT_DEBUGGER},
	{"gdb", 0, OPT_GDB},
};

static int
ProcessOption(OptID opt, char *arg)
{
	switch(opt) {
	case OPT_DEBUGGER:
		bootDebugger = 1;
		break;
	case OPT_GDB:
		sysDebugger->SetGDBMode(1);
		break;
	}
	return 0;
}

static int
ParseArguments()
{
	argc = 0;
	if (!pMBInfo || !(pMBInfo->flags & MBIF_CMDLINE)) {
		return 0;
	}
	trace("Command line: %s\n", pMBInfo->cmdLine);

	char *s = pMBInfo->cmdLine;
	/* create argv, argc */
	while (*s) {
		while (*s == ' ' || *s == '\t') {
			s++;
		}
		if (!*s) {
			break;
		}
		if (argc >= MAX_CMDLINE_PARAMS) {
			panic("Command line arguments number limit exceeded");
		}
		char *next = s;
		int qf = 0;
		while (*next) {
			if (qf) {
				if (*next != qf) {
					next++;
					continue;
				}
				break;
			}
			if (*next == '"' || *next == '\'') {
				qf = *next;
				next++;
				s = next;
				continue;
			}
			if (*next == ' ' || *next == '\t') {
				break;
			}
			next++;
		}
		argv[argc] = (char *)MM::malloc(next - s + 1, 1);
		memcpy(argv[argc], s, next - s);
		argv[argc][next - s] = 0;
		argc++;
		s = next;
		if (*s == '"' || *s == '\'') {
			s++;
		}
	}

	for (int i = 1; i < argc; i++) {
		char *o = argv[i];
		if (o[0] != '-' || o[1] != '-') {
			panic("Invalid command line format");
		}
		o += 2;
		int found = 0;
		for (u32 j = 0; j < sizeof(opts) / sizeof(opts[0]); j++) {
			if (!strcmp(o, opts[j].name)) {
				found = 1;
				char *arg;
				if (opts[j].hasArg) {
					if (i <= argc - 1) {
						panic("Missing argument for '%s' option", o);
					}
					arg = argv[i + 1];
					i++;
				} else {
					arg = 0;
				}
				ProcessOption(opts[j].id, arg);
				break;
			}
 		}
		if (!found) {
			panic("Invalid option specified: '%s'", o);
		}
	}

	return 0;
}

int MyTimer(Handle h, u64 ticks, void *arg)
{
	Time time;
	tm->GetTime(&time);
	printf("MyTimer: ticks = %llu, name = %s, time = %llu.%06lu\n",
		ticks, (char *)arg, time.sec, time.usec);
	return 0;
}

static int
InitConsoles()
{
	/* create default system console */
	ChrDevice *serial = (ChrDevice *)devMan.CreateDevice("ser");
	sysCons = (SysConsole *)devMan.CreateDevice("syscons", 0);
	sysCons->AddOutputDevice(serial);
	sysCons->SetInputDevice(serial);
	printf(copyright);
	/* Debugger console utilizes serial interface and should not use queuing */
	SysConsole *dbgCons = (SysConsole *)devMan.CreateDevice("syscons", 1);
	dbgCons->AddOutputDevice(serial);
	dbgCons->SetInputDevice(serial);
	return 0;
}

void
Main(paddr_t firstAddr)
{
	/* setup basic memory management */
	MM::PreInitialize((vaddr_t)(firstAddr - LOAD_ADDRESS + KERNEL_ADDRESS));
	/* call constructors for all static objects */
	CXA::ConstructStaticObjects();
	/* become noisy */
	InitConsoles();
	/* setup GDT, LDT, IDT */
	InitTables();
	/* create kernel built-in debugger */
	sysDebugger = NEWSINGLE(Debugger, (ConsoleDev *)devMan.GetDevice("syscons", 1));
	/* create system log */
	log = NEWSINGLE(Log);
	/* process command line parameters */
	ParseArguments();
	if (bootDebugger) {
		RunDebugger("Boot options requested debugger");
	}
	mm = NEWSINGLE(MM);
	/* from now kernel memory management is fully operational */
	im = NEWSINGLE(IM);
	/* ... as well as interrupts management */

	/* try to create text terminal on video device if available */
	ConsoleDev *videoTerm = (ConsoleDev *)devMan.CreateDevice("vga", 0);
	if (videoTerm) {
		sysCons->AddOutputDevice(videoTerm);
		videoTerm->Printf("%s\n", copyright);
		mm->PrintMemInfo(videoTerm);
	} else {
		klog(KLOG_INFO, "Video terminal could not be created");
	}

	/* system time, timers, deferred calls and so on */
	tm = NEWSINGLE(TM);

	/* Attach bootstrap processor and try to involve others */
	if (!CPU::Startup()) {
		panic("Unable to create device object for bootstrap CPU");
	}
	CPU::StartSMP(); /* XXX should be called when processes are initialized */

	sti();//temp
	tm->SetTimer(MyTimer, tm->GetTicks(), (void *)"periodic 2s", tm->MS(2000));
	tm->SetTimer(MyTimer, tm->GetTicks(), (void *)"periodic 3s", tm->MS(3000));
	tm->SetTimer(MyTimer, tm->GetTicks() + tm->MS(5000), (void *)"one-shot 5s");
	while (1) hlt();

	panic("Main exited");
	/* NOTREACHED */
}
