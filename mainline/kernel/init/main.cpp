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

static char *vfsRoot;

#define INIT_PATH	"/bin/init"
PM::Process *initProc;

static int
InitTables()
{
	idt = NEWSINGLE(IDT);
	gdt = NEWSINGLE(GDT);
	defTss = NEWSINGLE(TSS, &boot_stack);
	defTss->SetActive();
	return 0;
}

typedef enum {
	OPT_DEBUGGER,		/* Stop in debugger on boot */
	OPT_GDB,			/* Turn on remote GDB mode in debugger */
	OPT_ROOT,			/* Block device to mount on VFS root */
} OptID;

typedef struct {
	const char *name;
	int hasArg;
	OptID id;
} Option;

static Option opts[] = {
	{"debugger", 0, OPT_DEBUGGER},
	{"gdb", 0, OPT_GDB},
	{"root", 1, OPT_ROOT},
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
	case OPT_ROOT:
		vfsRoot = arg;
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
					if (i >= argc - 1) {
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

int MyTimer(Handle h, u64 ticks, void *arg)//temp
{
	Time time;
	tm->GetTime(&time);
	printf("MyTimer: ticks = %llu, name = %s, time = %llu.%06lu\n",
		ticks, (char *)arg, time.sec, time.usec);
	return 0;
}

static int proc1, proc2;

int MyProcess(void *arg)//temp
{
	printf("Process: %s\n", (char *)arg);
	while (1) {
		//pm->Wakeup(&proc1);
		pm->Sleep(&proc2, "proc2", tm->MS(1000));
		printf("process 1 waken\n");
	}
	return 0;
}

/* initial kernel thread */
static int
StartupProc(void *arg)
{
	/* now we are in the kernel process */
	devMan.ProbeDevices();

	/* wake up the rest CPUs */
	CPU::StartSMP();

	/* create VFS object */
	vfs = NEWSINGLE(VFS);

	/* mount root */
	if (!vfsRoot) {
		vfsRoot = (char *)"ramdisk0";
		klog(KLOG_INFO, "No root device specified, using default ('%s')", vfsRoot);
	}
	Device *rootDev = devMan.GetDevice(vfsRoot);
	if (!rootDev) {
		panic("Root device not found: %s", vfsRoot);
	}
	if (rootDev->GetType() != Device::T_BLOCK) {
		panic("Root device is not block device: %s", vfsRoot);
	}
	if (vfs->MountDevice((BlkDevice *)rootDev, "/")) {
		panic("Failed to mount root device: %s", vfsRoot);
	}

	/* launch 'init' application */
	initProc = pm->CreateProcess(INIT_PATH);
	if (!initProc) {
		klog(KLOG_WARNING, "Cannot launch 'init' application");
	}

	sti();//temp
	tm->SetTimer(MyTimer, tm->GetTicks(), (void *)"periodic 2s", tm->MS(2000));
	tm->SetTimer(MyTimer, tm->GetTicks(), (void *)"periodic 3s", tm->MS(3000));
	tm->SetTimer(MyTimer, tm->GetTicks() + tm->MS(5000), (void *)"one-shot 5s");

	pm->CreateProcess(MyProcess, (void *)"process 1");

	while (1) {
		//pm->Wakeup(&proc2);
		pm->Sleep(&proc1, "proc1", tm->MS(2000));
		printf("init proc waken\n");
	}
	return 0;
}

static int
SystemStartup(void *arg)
{
	/* gating management */
	gm = NEWSINGLE(GM);
	/* processes management */
	pm = NEWSINGLE(PM);
	pm->AttachCPU(StartupProc);
	/* NOT REACHED */
}

void Main(paddr_t firstAddr) __noreturn;

void
Main(paddr_t firstAddr)
{
	/* setup basic memory management */
	MM::PreInitialize((vaddr_t)(firstAddr - LOAD_ADDRESS + KERNEL_ADDRESS));
	/* setup GDT, LDT, IDT */
	InitTables();
	/* call constructors for all static objects */
	CXA::ConstructStaticObjects();
	/* become noisy */
	InitConsoles();
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
	sysCons->SetQueue(); /* activate console buffering */

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

	/* Attach and activate bootstrap processor */
	CPU *bsCpu = CPU::Startup();
	if (!bsCpu) {
		panic("Unable to create device object for bootstrap CPU");
	}
	bsCpu->Activate(SystemStartup);

	panic("Main exited");
}
