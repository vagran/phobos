/*
 * /phobos/bin/module_testing/module_test.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <module_test.h>

/**************************************************/
/* Test static objects construction */

class TestObj {
public:
	volatile u32 testProp;
	static volatile u32 testStaticProp;

	TestObj();
};

volatile u32 TestObj::testStaticProp = MT_DWORD_VALUE;

TestObj::TestObj()
{
	testProp = MT_DWORD_VALUE;
}

volatile TestObj testStaticObj;

void
TestStaticObj()
{
	mt_assert(testStaticObj.testStaticProp == MT_DWORD_VALUE);
	mt_assert(testStaticObj.testProp == MT_DWORD_VALUE);
}

/**************************************************/
/* Test initialized and uninitialized data segments */

#define TEST_BSS_SIZE (1024 * 1024)
volatile u8 testBssArea[TEST_BSS_SIZE];

volatile u32 cowTestArea = MT_DWORD_VALUE;

void
TestDataSegments()
{
	/* Test BSS area initialization */
	for (size_t i = 0; i < sizeof(testBssArea); i++) {
		mt_assert(!testBssArea[i]);
	}
	for (size_t i = 0; i < sizeof(testBssArea); i++) {
		testBssArea[i] = MT_BYTE_VALUE;
	}
	for (size_t i = 0; i < sizeof(testBssArea); i++) {
		mt_assert(testBssArea[i] == MT_BYTE_VALUE);
	}

	/* Test copy-on-write functionality */
	mt_assert(cowTestArea == MT_DWORD_VALUE);
	cowTestArea = MT_DWORD_VALUE2;
	mt_assert(cowTestArea == MT_DWORD_VALUE2);
}

/**************************************************/
/* Launch the child process - dynamic module testing binary */

#define NUM_DYN_ARGS	16

void
TestDyn()
{
	CString args;

	args.Format("%d", NUM_DYN_ARGS);
	for (int i = 0; i < NUM_DYN_ARGS; i++) {
		args.AppendFormat(" mt_arg%d", i);
	}
	GProcess *proc = uLib->GetApp()->CreateProcess("/bin/module_test_dyn",
		"Dynamical module testing binary", PM::DEF_PRIORITY, args);
	mt_assert(proc);
	proc->Release();
}

/**************************************************/

int
Main(GApp *app)
{
	/*
	 * Do basic low-level check here because we do not yet know if static
	 * objects constructors are functional.
	 */
	mt_assert(app);

	TestDataSegments();

	TestStaticObj();

	mtMan.RunTests();

#ifdef MT_STATIC
	TestDyn();
#endif

	mtlog("Module testing successfully finished");
	return 0;
}

/**************************************************/

MTestMan mtMan;

mtid_t MTestMan::nextId = 1;

Tree<mtid_t>::TreeRoot MTestMan::tests = TREE_ROOT_INIT;

u32 MTestMan::numTests = 0;

mtid_t MTestMan::curTestID = 0;

MTestMan::TestRegistrator::TestRegistrator(MTestFactory factory,
	const char *name, const char *desc)
{
	mtid_t id = GenID();
	if (MTestMan::RegisterTest(id, factory, name, desc)) {
		mtlog("Failed to register test '%s'", name);
	}
}

MTestMan::MTestMan()
{

}

MTestMan::~MTestMan()
{
	MTestEntry *te;
	while ((te = TREE_ROOT(MTestEntry, tree, tests))) {
		TREE_DELETE(tree, te, tests);
		DELETE(te);
	}
}

int
MTestMan::RegisterTest(mtid_t id, MTestFactory factory, const char *name,
		const char *desc)
{
	MTestEntry *te = NEW(MTestEntry);
	if (!te) {
		return -1;
	}
	memset(te, 0, sizeof(*te));
	TREE_ADD(tree, te, tests, id);
	te->name = name;
	te->desc = desc;
	te->factory = factory;
	return 0;
}

MTest *
MTestMan::GetCurTest()
{
	if (!curTestID) {
		return 0;
	}
	MTestEntry *te = GetTestEntry(curTestID);
	return te ? te->obj : 0;
}

MTestMan::MTestEntry *
MTestMan::GetTestEntry(mtid_t id)
{
	return TREE_FIND(id, MTestEntry, tree, tests);
}

void
MTestMan::SetTestObj(mtid_t id, MTest *obj)
{
	MTestEntry *te = GetTestEntry(id);
	te->obj = obj;
}

int
MTestMan::RunTests()
{
	MTestEntry *te;
	TREE_FOREACH(MTestEntry, tree, te, tests) {
		curTestID = TREE_KEY(tree, te);
		MTest *test = te->factory(curTestID, te->name, te->desc);
		/* whole work is done in MTest object constructor */
		te->obj = 0;
		DELETE(test);
	}
	curTestID = 0;
	return 0;
}
