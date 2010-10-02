/*
 * /phobos/kernel/sys/Fifo.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef FIFO_H_
#define FIFO_H_
#include <sys.h>
phbSource("$Id$");

template <class elem_t, int size, bool isCircular = 0>
class Fifo : public Object {
public:
	enum DropStatus {
		DS_DROP, /* Drop existing element, replace by new one */
		DS_PRESERVE, /* Preserve existing element, do not accept new one */
	};

	typedef DropStatus (*DropHdl)(elem_t *e, void *arg);
	typedef DropStatus (Object::*DropHdlMethod)(elem_t *e);
private:
	enum DropHandlerType {
		DHT_NONE,
		DHT_PLAIN,
		DHT_METHOD
	};

	u32 length, curIn, curOut;
	elem_t fifo[size];
	union {
		struct {
			DropHdl hdl;
			void *arg;
		} plain;
		struct {
			DropHdlMethod hdl;
			Object *obj;
		} method;
	} dropHdl;
	DropHandlerType dht;

	inline u32 &Advance(u32 &ptr) {
		assert(ptr < size);
		ptr++;
		if (ptr == size) {
			ptr = 0;
		}
		return ptr;
	}

	inline DropStatus Drop(elem_t *e) {
		if (dht == DHT_NONE) {
			return DS_DROP;
		}
		if (dht == DHT_PLAIN) {
			return dropHdl.plain.hdl(e, dropHdl.plain.arg);
		}
		return (dropHdl.method.obj->*dropHdl.method.hdl)(e);
	}

public:
	inline Fifo() {
		length = 0;
		curIn = 0;
		curOut = 0;
		dht = DHT_NONE;
	}

	inline u32 GetLenth() {
		return length;
	}

	inline void RegisterHandler(DropHdl hdl, void *arg = 0) {
		dropHdl.plain.hdl = hdl;
		dropHdl.plain.arg = arg;
		dht = DHT_PLAIN;
	}

	inline void RegisterHandler(DropHdlMethod hdl, Object *obj) {
		dropHdl.method.hdl = hdl;
		dropHdl.method.obj = obj;
		dht = DHT_METHOD;
	}

	/* return non-zero if failed (e.g. drop handler returned DS_PRESERVE) */
	inline int Push(elem_t e) {
		if (length < size) {
			fifo[curIn] = e;
			Advance(curIn);
			length++;
			return 0;
		}
		if (!isCircular) {
			return -1;
		}
		if (Drop(&fifo[curOut]) != DS_DROP) {
			return -1;
		}
		Advance(curOut);
		fifo[curIn] = e;
		Advance(curIn);
		return 0;
	}

	inline int Pull(elem_t *e) {
		if (!length) {
			return -1;
		}
		*e = fifo[curOut];
		Advance(curOut);
		length--;
		return 0;
	}

	inline void Reset() {
		curIn = 0;
		curOut = 0;
		length = 0;
	}
};

#endif /* FIFO_H_ */
