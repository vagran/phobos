/*
 * /phobos/bin/module_testing/module_test.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef MODULE_TEST_H_
#define MODULE_TEST_H_
#include <sys.h>
phbSource("$Id$");

enum MTestValues {
	MT_BYTE_VALUE =		(u8)42, /* The meaning of the life */
	MT_BYTE_VALUE2 =	(u8)~MT_BYTE_VALUE,
	MT_WORD_VALUE =		(u16)0x1234,
	MT_WORD_VALUE2 =	(u16)~MT_WORD_VALUE,
	MT_DWORD_VALUE =	(u32)0x13245768,
	MT_DWORD_VALUE2 =	(u32)~MT_DWORD_VALUE,
};

/******************************************************************************/

typedef u32 mtid_t;

class MTest;

typedef MTest *(*MTestFactory)(mtid_t id, const char *name, const char *desc);

class MTestMan {
public:
	class TestRegistrator {
	private:
	public:
		TestRegistrator(MTestFactory factory, const char *name, const char *desc = 0);
	};
private:
	typedef struct {
		Tree<mtid_t>::TreeEntry tree;
		MTestFactory factory;
		const char *name, *desc;
		MTest *obj;
	} MTestEntry;

	static mtid_t nextId;
	static Tree<mtid_t>::TreeRoot tests;
	static u32 numTests;
	static mtid_t curTestID;

	static MTestEntry *GetTestEntry(mtid_t id);
public:
	MTestMan();
	~MTestMan();
	static mtid_t GenID() { return AtomicOp::Add(&nextId, 1); }
	static int RegisterTest(mtid_t id, MTestFactory factory, const char *name,
		const char *desc = 0);
	static mtid_t GetCurTestID() { return curTestID; }
	static void SetTestObj(mtid_t id, MTest *obj);
	static MTest *GetCurTest();

	int RunTests();
};

extern MTestMan mtMan;

class MTest {
protected:
	mtid_t id;
	const char *name, *desc;
public:
	MTest(mtid_t id, const char *name, const char *desc = 0) {
		this->id = id;
		this->name = name;
		this->desc = desc;
		MTestMan::SetTestObj(id, this);
	}
	virtual ~MTest() {}

	mtid_t GetID() { return id; }
	const char *GetName() { return name; }
	const char *GetDesc() { return desc; }
};

#define MT_DECLARE(className) \
	className(mtid_t id, const char *name, const char *desc = 0); \
	virtual ~className(); \
	static MTest *_MTFactory(mtid_t id, const char *name, const char *desc = 0) { \
		return NEW(className, id, name, desc); \
	}

#define MT_DEFINE(className, testName, ...) \
	static MTestMan::TestRegistrator __UID(MTestRegistrator) \
		(className::_MTFactory, testName, ## __VA_ARGS__); \
	className::className(mtid_t id, const char *name, const char *desc) : \
		MTest(id, name, desc)

#define MT_DESTR(className) className::~className()

/******************************************************************************/

inline void __mt_logv(const char *func, u32 line, const char *fmt, va_list args)
	__format(printf, 3, 0);
inline void
__mt_logv(const char *func, u32 line, const char *fmt, va_list args)
{
	CString s = "Module testing";
	MTest *test = MTestMan::GetCurTest();
	if (test) {
		s.AppendFormat(" (%s):", test->GetName());
	} else {
		s += ':';
	}
	s.AppendFormatV(fmt, args);
	printf("%s\n", s.GetBuffer());
}

inline void __mt_log(const char *func, u32 line, const char *fmt,...)
	__format(printf, 3, 4);
inline void
__mt_log(const char *func, u32 line, const char *fmt,...)
{
	va_list args;
	va_start(args, fmt);
	__mt_logv(func, line, fmt, args);
}

#define mtlog(fmt, ...) __mt_log(__func__, __LINE__, fmt, ## __VA_ARGS__)

#define mtlogv(fmt, args) _klogv(__func__, __LINE__, fmt, args)

inline void
__mt_assert(const char *file, u32 line, const char *cond)
{
	mtlog("Module testing assert failed at '%s@%lu': '%s'", file, line, cond);
	uLib->GetApp()->ExitThread(MT_DWORD_VALUE2);
}

#define mt_assert(condition) { if (!(condition)) \
	__mt_assert(__FILE__, __LINE__, __STR(condition)); }

#endif /* MODULE_TEST_H_ */
