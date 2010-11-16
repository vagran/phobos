/*
 * /phobos/bin/init/init.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

int myGlobalBSS, myGlobalInit = 237;

int
Main(GApp *app)
{
	int rc;
	u8 c;
	CString s;

	GProcess *proc = app->GetProcess();
	CString str(GETSTR(proc, GProcess::GetName));
	str += ' ';
	str += GETSTR(proc, GProcess::GetName);
	str += '\n';
	str += "0123456789012345678901234567890123456789012345678901234567890123456789\n";
	GStream *out = app->GetStream("output");
	out->Write((u8 *)str.GetBuffer(), str.GetLength());
	GTime *time = app->GetTime();

	GStream *in = app->GetStream("input");

	void *buf = app->AllocateHeap(1500);
	if (!buf) {
		s = "Heap allocation failed\n";
		out->Write((u8 *)s.GetBuffer(), s.GetLength());
		s = GETSTR(app, GApp::GetLastErrorStr);
		out->Write((u8 *)s.GetBuffer(), s.GetLength());
	} else {
		s.Format("buf = 0x%08lx\n", (u32)buf);
		out->Write((u8 *)s.GetBuffer(), s.GetLength());
	}
	memset(buf, 0xed, 1500);
	u32 size = app->GetHeapSize(buf);
	s.Format("Size = %ld\n", size);
	out->Write((u8 *)s.GetBuffer(), s.GetLength());

	while (1) {
		GateObject *objs[] = { in };
		u32 bitmap;

		if (app->Wait(GateObject::OP_READ, objs, SIZEOFARRAY(objs), &bitmap,
			time->S(2))) {
			s = "Wait failed\n";
			out->Write((u8 *)s.GetBuffer(), s.GetLength());
			s = GETSTR(app, GApp::GetLastErrorStr);
			out->Write((u8 *)s.GetBuffer(), s.GetLength());
		} else {
			if (BitIsSet(&bitmap, 0)) {
				rc = in->Read(&c, 1);
				if (rc < 0) {
					s = "Read failed\n";
					out->Write((u8 *)s.GetBuffer(), s.GetLength());
				} else if (rc) {
					s.Format("Character read: 0x%02lx ('%c')\n", (u32)c, c);
					out->Write((u8 *)s.GetBuffer(), s.GetLength());
				}
			} else {
				s = "Waken by timeout\n";
				out->Write((u8 *)s.GetBuffer(), s.GetLength());
			}
		}
	}

	return proc->GetPID();//temp
}
