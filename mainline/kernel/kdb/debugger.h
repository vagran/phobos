/*
 * /kernel/kdb/debugger.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef DEBUGGER_H_
#define DEBUGGER_H_
#include <sys.h>
phbSource("$Id$");

class Debugger {
private:
	enum {
		MAX_CMD_LINE = 4096,
		MAX_CMD_ARGS = 64,
	};

	typedef enum {
		GR_EAX,
		GR_ECX,
		GR_EDX,
		GR_EBX,
		GR_ESP,
		GR_EBP,
		GR_ESI,
		GR_EDI,
		GR_EIP,
		GR_EFLAGS,
		GR_CS,
		GR_SS,
		GR_DS,
		GR_ES,
		GR_FS,
		GR_GS,
		GR_MAX
	} GDBRegs;

	typedef enum {
		HS_OK,
		HS_ERROR,
		HS_EXIT,		/* exit from command loop */
	} HdlStatus;
	typedef HdlStatus (Debugger::*CmdHandler)(char **argv, u32 argc);

	typedef struct {
		const char *name;
		CmdHandler hdl;
	} CmdDesc;

	static CmdDesc cmds[];

	ConsoleDev *con;
	Frame *frame;
	u32 esp;
	u16 ss;
	char lineBuf[MAX_CMD_LINE];
	u32 lbSize;
	char *argv[MAX_CMD_ARGS];
	u32 argc;
	int gdbMode;
	int gdbSkipCont;
	int requestedBreak;

	static int _BPHandler(u32 idx, Debugger *d, Frame *frame);
	static int _DebugHandler(u32 idx, Debugger *d, Frame *frame);
	int BPHandler(Frame *frame);
	int Shell();
	void Prompt();
	int GetLine();
	int GetPacket();
	int PrintFrameInfo(Frame *frame);
	void ParseLine();
	CmdDesc *FindCommand(char *s);
	int GDB();
	void GDBSend(const char *data);
	int GetHex(u8 d);
	int ProcessPacket();
	int ReadRegisters();
	int DumpData(char **buf, void *data, u32 size);
	u32 FetchData(char **buf, void *data, u32 size);
	int ReadMemory();
	int WriteMemory();
	int SendStatus();
	int Continue();
	int WriteRegisters();
	int Step();

	/* commands handlers */
	HdlStatus cmd_continue(char **argv, u32 argc);
	HdlStatus cmd_step(char **argv, u32 argc);
	HdlStatus cmd_registers(char **argv, u32 argc);
	HdlStatus cmd_gdb(char **argv, u32 argc);
public:
	Debugger(ConsoleDev *con);

	int SetConsole(ConsoleDev *con);
	int Break();
	int SetGDBMode(int f);
	int Trap(IDT::SysTraps idx, Frame *frame);
};

extern Debugger *sysDebugger;

#endif /* DEBUGGER_H_ */
