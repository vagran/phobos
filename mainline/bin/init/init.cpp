/*
 * /phobos/bin/init/init.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
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
	GStream *out = app->GetStream("output");
	GTime *time = app->GetTime();
	GStream *in = app->GetStream("input");

	while (1) {
		GateObject *objs[] = { in };
		u32 bitmap;

		if (app->Wait(GateObject::OP_READ, objs, SIZEOFARRAY(objs), &bitmap,
			time->S(2)) < 0) {
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
