/*
 * /phobos/kernel/gate/GateStream.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef GATESTREAM_H_
#define GATESTREAM_H_
#include <sys.h>
phbSource("$Id$");

DECLARE_GCLASS(GStream);

class GStream : public GateObject {
public:
	enum ErrorCode {
			EOF = -1,
			E_FAULT = -0x7fff, /* Some general fault */
			E_NOTSUPPORTED, /* The stream doesn't support either reading or writing */
	};

private:
	friend class PM::Process;

	char *streamName;
	StringTree<>::TreeEntry nameTree;

public:
	GStream(const char *name);
	virtual ~GStream();

	virtual int GetName(char *buf = 0, int bufSize = 0);
	/* return number of bytes read, EOF if end of stream, another negative value if error */
	virtual int Read(u8 *buf, u32 size) { return E_NOTSUPPORTED; }
	/* return number of bytes written, negative value if error */
	virtual int Write(u8 *buf, u32 size) { return E_NOTSUPPORTED; }

	DECLARE_GCLASS_IMP(GStream);
};

#endif /* GATESTREAM_H_ */
