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
	"Copyright ©AST 2009\n";

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

void
Main(paddr_t firstAddr)
{
	/* setup basic memory management */
	MM::PreInitialize((vaddr_t)(firstAddr - LOAD_ADDRESS + KERNEL_ADDRESS));
	/* call constructors for all static objects */
	CXA::ConstructStaticObjects();
	/* create default system console */
	sysCons = (ConsoleDev *)devMan.CreateDevice("syscons");
	printf(copyright);
	InitTables();
	sysDebugger = NEWSINGLE(Debugger, sysCons);
	ParseArguments();
	if (bootDebugger) {
		RunDebugger("Boot options requested debugger");
	}
	mm = NEWSINGLE(MM);
	/* from now kernel memory management is fully operational */

	void *mem[1024];
	for (size_t i = 0; i < sizeof(mem) / sizeof(mem[0]); i++) {
		mem[i] = MM::malloc(16384);
		if (!mem[i]) {
			panic("MM::malloc() failed");
		}
		memset(mem[i], 237, 16384);
	}
	for (size_t i = 0; i < sizeof(mem) / sizeof(mem[0]); i++) {
		MM::mfree(mem[i]);
	}
	for (size_t i = 0; i < sizeof(mem) / sizeof(mem[0]); i++) {
		mem[i] = NEW(u64);
		if (!mem[i]) {
			panic("MM::malloc() failed");
		}
		memset(mem[i], 237, sizeof(u64));
	}
	for (size_t i = 0; i < sizeof(mem) / sizeof(mem[0]); i++) {
		DELETE((u64 *)mem[i]);
	}


	panic("Main exited");
	/* NOTREACHED */
}
