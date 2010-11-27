/*
 * /kernel/kern/pm.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef PM_H_
#define PM_H_
#include <sys.h>
phbSource("$Id$");

/* Processes manager */

/* Get per-CPU runqueue */
#define CPU_RQ(cpu)		((Runqueue *)cpu->pcpu.runQueue)

class GStream;

class PM : public Object {
public:
	typedef u16 pid_t;
	typedef u32 waitid_t;

	enum ProcessFault {
		PFLT_NONE,
		PFLT_GATE_OBJ, /* Invalid gate object passed to system call */
		PFLT_GATE_OBJ_RELEASE, /* Gate object release called without matching AddRef() */
		PFLT_GATE_METHOD, /* Invalid gate method called */
		PFLT_GATE_STACK, /* Invalid stack pointer when called gate method */
		PFLT_GATE_METHOD_RESTICTED, /* Restricted gate method called */
		PFLT_PAGE_FAULT, /* Page fault */
		PFLT_INVALID_BUFFER, /* Invalid user buffer provided to gate method */
		PFLT_ABORT, /* Aborted by application request */
	};

	enum {
		MAX_PROCESSES =		65536,
		NUM_PRIORITIES =	128,
		DEF_PRIORITY =		NUM_PRIORITIES / 2,
		KERNEL_PRIORITY =	DEF_PRIORITY,
		IDLE_PRIORITY =		NUM_PRIORITIES - 1,
		MAX_SLICE =			100, /* maximal time slice for a thread in ms */
		MIN_SLICE =			10, /* minimal time slice for a thread in ms */
		INVALID_PID =		(pid_t)~0,
	};

	class ImageLoader : public Object {
	protected:
		RefCount refCount;
		VFS::File *file;
	public:
		ImageLoader(VFS::File *file);
		virtual ~ImageLoader();

		OBJ_ADDREF(refCount);
		OBJ_RELEASE(refCount);

		virtual int IsInterp(KString *interp) { return 0; }
		virtual int Load(MM::Map *map, MM::VMObject *bssObj) = 0;
		virtual vaddr_t GetEntryPoint() = 0;
	};

	typedef ImageLoader *(*ILFactory)(VFS::File *file);
	typedef int (*ILProber)(VFS::File *file); /* return zero if identified */

	class ILRegistrator : public Object {
	public:
		ILRegistrator(const char *desc, ILFactory factory, ILProber prober);
	};
public:
	class Thread;
	class Process;
	class Runqueue;

	typedef struct {
		Tree<pid_t>::TreeEntry tree; /* key is PID */
		u32	isThread:1;
		union {
			Thread *thrd;
			Process *proc;
		};
	} PIDEntry;

	class Thread : public Object {
	public:
		enum {
			DEF_STACK_SIZE =		16 * 1024 * 1024, /* Default stack size */
			DEF_KERNEL_STACK_SIZE =	8 * 1024 * 1024, /* Default stack size for kernel threads */
		};

		enum State {
			S_NONE,
			S_RUNNING,
			S_SLEEP,
			S_TERMINATED,
			S_STOPPED,
		};

		typedef int (*ThreadEntry)(void *arg);

		typedef struct {
			u32 esp, ebp, eip;
			/* XXX FPU state */
		} Context;
	private:
		friend class Process;
		friend class Runqueue;
		friend class PM;

		enum {
			NUM_STATIC_WAIT_ENTRIES =	32,
			ERROR_HISTORY_SIZE =		2,
		};

		ListEntry list; /* list of threads in process */
		ListEntry rqList; /* entry in run-queue */
		PIDEntry pid;
		u32 rqFlags;
		Process *proc;
		u32 stackSize;
		vaddr_t gateCallSP; /* stack pointer on gate method entrance, kernel stack starts from there */
		vaddr_t gateCallRetAddr; /* application return address from a system call */
		Frame *trapFrame;
		MM::VMObject *stackObj;
		MM::Map::Entry *stackEntry;
		CPU *cpu;
		Context ctx;
		u32 priority;
		u32 sliceTicks; /* ticks of time slice left */
		ListHead waitEntries; /* empty if not waiting, list of ThreadElements */
		int numWaitEntries;
		const char *waitString;
		Handle waitTimeout;
		waitid_t wakenBy; /* by timeout when zero */
		void *rqQueue; /* active or expired queue */
		State state;
		int exitCode;
		ProcessFault fault;
		KString faultStr;
		int isActive; /* currently running on some CPU */
		Error err[ERROR_HISTORY_SIZE];
		int curError; /* current error object index */
		int numErrors; /* number of valid errors in history (including current one) */
		GateObject *gateObj; /* gate object which handles current system call, 0 if not in system call */
	public:
		Thread(Process *proc);
		~Thread();

		int Initialize(ThreadEntry entry, void *arg = 0,
			u32 stackSize = DEF_STACK_SIZE, u32 priority = DEF_PRIORITY);
		void Exit(int exitCode) __noreturn;
		int SaveContext(Context *ctx); /* return 0 for caller, 1 for restored thread */
		void RestoreContext(Context *ctx, u32 asRoot = 0); /* jump to SaveContext */
		void SwitchTo(); /* thread must be in same CPU runqueue with the current thread */
		static Thread *GetCurrent();
		int Run(CPU *cpu = 0);
		int Stop();
		int Sleep(const char *waitString, Handle waitTimeout = 0);
		int Unsleep();
		int Terminate(int exitCode = 0);
		inline Runqueue *GetRunqueue() { return cpu ? CPU_RQ(cpu) : 0; }
		inline Process *GetProcess() { return proc; }
		inline pid_t GetID() { return TREE_KEY(tree, &pid); }
		int MapKernelStack(vaddr_t esp);
		inline int IsValidSP(vaddr_t esp) {
			return esp > stackEntry->base &&
				esp <= stackEntry->base + stackEntry->size;
		}
		inline void SetGateCallSP(u32 esp) {
			gateCallSP = esp;
			gateCallRetAddr = *(vaddr_t *)esp;
		}
		inline void SetTrapFrame(Frame *frame) {
			trapFrame = frame;
		}
		inline void SetGateObject(GateObject *gateObj) { this->gateObj = gateObj; }
		int IsKernelStack(vaddr_t addr, vsize_t size);
		int Fault(ProcessFault flt, const char *msg = 0, ...) __format(printf, 3, 4);
		Error *GetError(int depth = 0); /* depth 0 for current error, 1 for previous and so on */
		Error *NextError(); /* Advance current error to the next one in the history ring */
		Error *PrevError(); /* Advance current error to the previous one in the history ring */
	};

	class Process : public Object {
	public:
		enum State {
			S_RUNNING,
			S_STOPPED,
			S_TERMINATED,
		};
	private:
		friend class PM;

		ListEntry list; /* list of all processes in PM */
		PIDEntry pid;
		ListHead threads;
		u32 numThreads, numAliveThreads;
		SpinLock thrdListLock;
		MM::Map *map, *userMap, *gateMap;
		MM::VMObject *bssObj, *heapObj;
		u32 priority;
		vaddr_t entryPoint;
		GM::GateArea *gateArea;
		KString faultStr;
		State state;
		ProcessFault fault;
		KString name;
		SpinLock streamsLock;
		StringTree<>::TreeRoot streams;
		int isKernelProc;
		KString args;

		void SetEntryPoint(vaddr_t ep) { entryPoint = ep; }
		int DeleteThread(Thread *t);
		MM::Map::Entry *GetHeapEntry(vaddr_t va);
	public:
		Process();
		~Process();

		static inline Process *GetCurrent() {
			Thread *thrd = Thread::GetCurrent();
			return thrd ? thrd->GetProcess() : 0;
		}

		int Initialize(u32 priority, const char *name = 0, int isKernelProc = 0);
		Thread *CreateThread(Thread::ThreadEntry entry, void *arg = 0,
			u32 stackSize = Thread::DEF_STACK_SIZE, u32 priority = DEF_PRIORITY);
		int TerminateThread(Thread *thrd, int exitCode = 0);
		inline pid_t GetID() { return TREE_KEY(tree, &pid); }
		inline KString *GetName() { return &name; }
		Thread *GetThread();
		inline MM::Map *GetMap() { return map; }
		inline MM::Map *GetUserMap() { return userMap; }
		inline MM::Map *GetGateMap() { return gateMap; }
		inline GM::GateArea *GetGateArea() { return gateArea; }
		int Stop();
		int Resume();
		int Fault(ProcessFault flt, const char *msg = 0, ...) __format(printf, 3, 4);
		int AddStream(GStream *stream);
		int RemoveStream(GStream *stream);
		GStream *GetStream(const char *name);
		int CheckUserBuf(void *buf, u32 size, MM::Protection protection = MM::PROT_READ);
		int CheckUserString(const char *str);
		void *AllocateHeap(u32 size, int prot = MM::PROT_READ | MM::PROT_WRITE);
		int FreeHeap(void *p);
		u32 GetHeapSize(void *p);
		inline KString *GetArgs() { return &args; }
	};

	class Runqueue : public Object {
	public:
		enum ThreadFlags {
			TF_EXPIRED =		0x1,
		};
	private:

		inline u32 GetSliceTicks(Thread *thrd);
	public:
		ListEntry list;
		SpinLock qLock;
		typedef struct {
			ListHead queue[NUM_PRIORITIES]; /* runnable threads */
		} Queue;
		Queue queues[2];
		Queue *qActive, *qExpired;
		u8 activeMask[NUM_PRIORITIES / NBBY], queuedMask[NUM_PRIORITIES / NBBY];
		CPU *cpu;
		Thread *curThread;
		u32 numQueued, numActive;

		Runqueue();
		int AddThread(Thread *thrd);
		int RemoveThread(Thread *thrd);
		Thread *SelectThread(); /* select next thread to run */
		inline Thread *GetCurrentThread() { return curThread; }
	};

	static int RegisterIL(const char *desc, ILFactory factory, ILProber prober);
private:
	typedef struct {
		ListEntry list;
		const char *desc;
		ILFactory factory;
		ILProber prober;
	} ILEntry;

	typedef struct {
		enum Flags {
			F_WAKEN =		0x1,
		};

		Tree<waitid_t>::TreeEntry tree;
		u32 flags;
		ListHead threads; /* List of ThreadElement structures */
		u32 numThreads;
	} SleepEntry;

	/*
	 * Intermediate structure to implement many-to-many relations between
	 * threads and sleep entries
	 */
	typedef struct {
		ListEntry seList;
		ListEntry thrdList;
		Thread *thread;
		SleepEntry *se;
	} ThreadElement;

	static ListHead imageLoaders;
	ListHead processes;
	SpinLock rqLock;
	ListHead runQueues;
	u32 numProcesses;
	Tree<pid_t>::TreeRoot pids;
	SpinLock procListLock;
	Bitmap pidMap;
	SpinLock pidMapLock;
	SpinLock tqLock;
	Tree<waitid_t>::TreeRoot tqSleep; /* sleeping threads keyed by waiting channel ID */
	Process *kernelProc;
	Thread *kernInitThread;

	pid_t AllocatePID();
	int ReleasePID(pid_t pid);
	static int IdleThread(void *arg);
	void IdleThread() __noreturn;
	/* Do not create a thread if 'entry' is zero */
	Process *IntCreateProcess(Thread::ThreadEntry entry, void *arg = 0,
		const char *name = 0, int priority = DEF_PRIORITY, int isKernelProc = 0,
		int runIt = 1);
	SleepEntry *GetSleepEntry(waitid_t id); /* must be called with tqLock */
	SleepEntry *CreateSleepEntry(waitid_t id); /* must be called with tqLock */
	void FreeSleepEntry(SleepEntry *p); /* must be called with tqLock */
	static int SleepTimeout(Handle h, u64 ticks, void *arg);
	void SleepTimeout(Thread *thrd);
	int Wakeup(SleepEntry *se);
	int Wakeup(Thread *thrd);
	int UnsleepThread(Thread *thrd);
	ImageLoader *GetImageLoader(VFS::File *file);
	static int ProcessEntry(void *arg) __noreturn;
public:
	PM();
	static inline Thread *GetCurrentThread() { return Thread::GetCurrent(); }
	Process *CreateProcess(Thread::ThreadEntry entry, void *arg = 0,
		const char *name = 0, int priority = DEF_PRIORITY);
	Process *CreateProcess(const char *path, const char *name = 0,
		int priority = DEF_PRIORITY, const char *args = 0);
	int DestroyProcess(Process *proc);
	void AttachCPU(Thread::ThreadEntry kernelProcEntry = 0, void *arg = 0) __noreturn;
	inline Process *GetKernelProc() { return kernelProc; }
	/*
	 * If byTimeout argument is provided it indicates either the thread was waken by timeout
	 * or by event on specified channel.
	 */
	int Sleep(waitid_t channelID, const char *sleepString, u64 timeout = 0, waitid_t *wakenBy = 0);
	int Sleep(void *channelID, const char *sleepString, u64 timeout = 0, void **wakenBy = 0);
	int Sleep(waitid_t *channelsID, int numChannels,
		const char *sleepString, u64 timeout = 0, waitid_t *wakenBy = 0);
	int Sleep(void **channelsID, int numChannels,
			const char *sleepString, u64 timeout = 0, void **wakenBy = 0);
	int ReserveSleepChannel(void *channelID);
	int ReserveSleepChannel(waitid_t channelID);
	int FreeSleepChannel(void *channelID); /* do not need to be called if Sleep() was called */
	int FreeSleepChannel(waitid_t channelID); /* do not need to be called if Sleep() was called */
	int Wakeup(waitid_t channelID);
	int Wakeup(void *channelID);

	int ValidateState();
	CPU *SelectCPU(); /* select the most suitable CPU for new task */
	static const char *StrProcessFault(ProcessFault flt);
};

#define ERROR(code,...) { \
	PM::Thread *__thrd = PM::Thread::GetCurrent(); \
	if (__thrd) { \
		Error *__e = __thrd->GetError(); \
		if (__e) { \
			__e->RaiseError(__func__, __LINE__, Error::code, ## __VA_ARGS__); \
		} \
	} \
}

extern PM *pm;

#define DeclareILFactory() static PM::ImageLoader *_ILFactory(VFS::File *file)

#define DefineILFactory(className) PM::ImageLoader * \
className::_ILFactory(VFS::File *file) \
{ \
	PM::ImageLoader *il = NEWSINGLE(className, file); \
	if (!il) { \
		return 0; \
	} \
	return il; \
}

#define DeclareILProber() static int _ILProber(VFS::File *file)

#define DefineILProber(className) int className::_ILProber(VFS::File *file)

#define RegisterImageLoader(className, desc) static PM::ILRegistrator \
	__UID(ILRegistrator)(desc, className::_ILFactory, className::_ILProber)

#endif /* PM_H_ */
