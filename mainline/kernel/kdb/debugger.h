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
		DRQ_STOP,
		DRQ_CONTINUE,
	} DebugRequest;

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

	typedef struct {
		enum State {
			S_NONE,
			S_RUNNING,
			S_STOPPED,
		};

		ListEntry list;
		u32 id;
		CPU *cpu;
		State state;
		Frame *frame;
		int inRunLoop;
	} Thread;

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
	CPU *curCpu;
	SpinLock reqSendLock;
	DebugRequest curReq;
	u32 reqRef, reqValid;
	SpinLock reqLock;
	ListHead threads;
	u32 numThreads;
	SpinLock threadLock;
	Thread *curThread;

	static int _BPHandler(Frame *frame, Debugger *d);
	static int _DebugHandler(Frame *frame, Debugger *d);
	static int _SMPDbgReqHandler(Frame *frame, Debugger *d);
	int BPHandler(Frame *frame);
	int Shell();
	void Prompt();
	int GetLine();
	int GetPacket();
	int PrintFrameInfo(Frame *frame);
	void ParseLine();
	CmdDesc *FindCommand(char *s);
	int GDB();
	void GDBSend(const char *data, int reqAck = 1);
	int GetHex(u8 d);
	int ProcessPacket();
	int ReadRegisters();
	int DumpData(char **buf, u32 bufSize, const void *data, u32 size); /* returns number of bytes read from data */
	u32 FetchData(char **buf, void *data, u32 size);
	int ReadMemory();
	int WriteMemory();
	int SendStatus(Frame *frame);
	int Continue();
	int WriteRegisters();
	int Step();
	int Query();
	int GDBThreadState();
	int GDBSetThread();
	int DRHandler(Frame *frame);
	int SendDebugRequest(DebugRequest req, CPU *cpu = 0); /* if cpu == 0, broadcast request to all others */
	Thread *GetThread();
	Thread::State SetThreadState(Thread *thread, Thread::State state, Frame *frame = 0);
	HdlStatus StopLoop(int printStatus = 1);
	int RunLoop(Frame *frame, int printStatus = 1);
	Thread *FindThread(u32 id);
	HdlStatus SwitchThread(u32 id, int printStatus = 0);
	int SetFrame(Frame *frame);
	void GDBTrace(char *buf);

	/* commands handlers */
	HdlStatus cmd_continue(char **argv, u32 argc);
	HdlStatus cmd_step(char **argv, u32 argc);
	HdlStatus cmd_registers(char **argv, u32 argc);
	HdlStatus cmd_gdb(char **argv, u32 argc);
	HdlStatus cmd_listthreads(char **argv, u32 argc);
	HdlStatus cmd_thread(char **argv, u32 argc);
public:
	static int debugFaults;
	static int debugPanics;

	Debugger(ConsoleDev *con);

	int SetConsole(ConsoleDev *con);
	int Break();
	int SetGDBMode(int f);
	int Trap(Frame *frame);
	int Trace(const char *fmt,...) __format(printf, 2, 3);
	int VTrace(const char *fmt, va_list va) __format(printf, 2, 0);
};

extern Debugger *sysDebugger;

#endif /* DEBUGGER_H_ */
