/*
 * /kernel/lib/gcc.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#include <sys.h>
phbSource("$Id$");

#ifdef KERNEL
/* For application binaries it is defined in startup library */
void *__dso_handle;
#endif /* KERNEL */

extern "C" CXA::CallbackList _CTOR_LIST, _DTOR_LIST;

ASMCALL int
__cxa_atexit(void (*func)(void *), void * arg, void *dso_handle)
{
#if !defined(KERNEL) && defined (__PIC)
	static RTLinker *linker;
	if (!linker) {
		if (!GetDSO(&linker)) {
			return -1;
		}
	}
	linker->AtExit(*(RTLinker::DSOHandle *)dso_handle, func, arg);
#endif /* !defined(KERNEL) && defined (__PIC) */
	return 0;
}

ASMCALL void
__cxa_pure_virtual()
{
#ifdef KERNEL
    panic("Pure virtual function called");
#else
    //notimpl
#endif
}

ASMCALL void
__cxa_finalize(void *f)
{

}

int
CXA::ConstructStaticObjects()
{
	for (u32 i = 0; i < _CTOR_LIST.n; i++) {
		_CTOR_LIST.func[i]();
	}
	return 0;
}

int
CXA::DestructStaticObjects()
{
	for (u32 i = 0; i < _DTOR_LIST.n; i++) {
		_DTOR_LIST.func[i]();
	}
	return 0;
}

ASMCALL i64
__divdi3(i64 a, i64 b)
{
	u64 ua, ub, uq;
	int neg;

	if (a < 0) {
		ua = -(u64)a, neg = 1;
	} else {
		ua = a, neg = 0;
	}
	if (b < 0) {
		ub = -(u64)b, neg ^= 1;
	} else {
		ub = b;
	}
	uq = __qdivrem(ua, ub, (u64 *)0);
	return (neg ? -uq : uq);
}

ASMCALL u64
__udivdi3(u64 a,u64 b)
{

	return (__qdivrem(a, b, (u64 *)0));
}

ASMCALL i64
__moddi3(i64 a, i64 b)
{
	u64 ua, ub, ur;
	int neg;

	if (a < 0) {
		ua = -(u64)a, neg = 1;
	} else {
		ua = a, neg = 0;
	}
	if (b < 0) {
		ub = -(u64)b;
	} else {
		ub = b;
	}
	(void)__qdivrem(ua, ub, &ur);
	return (neg ? -ur : ur);
}

ASMCALL u64
__umoddi3(u64 a, u64 b)
{
	u64 r;

	__qdivrem(a, b, &r);
	return (r);
}

#define	CHAR_BIT	NBBY

/*
 * Total number of bits in a quad_t and in the pieces that make it up.
 * These are used for shifting, and also below for halfword extraction
 * and assembly.
 */
#define	QUAD_BITS	(sizeof(quad_t) * CHAR_BIT)
#define	LONG_BITS	(sizeof(u32) * CHAR_BIT)
#define	HALF_BITS	(sizeof(u32) * CHAR_BIT / 2)

/*
 * Extract high and low shortwords from longword, and move low shortword of
 * longword to upper half of long, i.e., produce the upper longword of
 * ((quad_t)(x) << (number_of_bits_in_long/2)).  (`x' must actually be u_long.)
 *
 * These are used in the multiply code, to split a longword into upper
 * and lower halves, and to reassemble a product as a quad_t, shifted left
 * (sizeof(long)*CHAR_BIT/2).
 */
#define	HHALF(x)	((x) >> HALF_BITS)
#define	LHALF(x)	((x) & ((1 << HALF_BITS) - 1))
#define	LHUP(x)		((x) << HALF_BITS)

#define	B	(1 << HALF_BITS)	/* digit base */

/* Combine two `digits' to make a single two-digit number. */
#define	COMBINE(a, b) (((u32)(a) << HALF_BITS) | (b))

typedef unsigned short digit;

union uu {
	i64	q;
	u64	uq;
	i32	sl[2];
	u32	ul[2];
};

/*
 * Define high and low longwords.
 */
#define	H		1
#define	L		0

/*
 * Shift p[0]..p[len] left `sh' bits, ignoring any bits that
 * `fall out' the left (there never will be any such anyway).
 * We may assume len >= 0.  NOTE THAT THIS WRITES len+1 DIGITS.
 */
static void
__shl(register digit *p, register int len, register int sh)
{
	register int i;

	for (i = 0; i < len; i++) {
		p[i] = LHALF(p[i] << sh) | (p[i + 1] >> (HALF_BITS - sh));
	}
	p[i] = LHALF(p[i] << sh);
}

ASMCALL u64
__qdivrem(u64 uq, u64 vq, u64 *arq)
{
	union uu tmp;
	digit * u, *v, *q;
	register digit v1, v2;
	u32 qhat, rhat, t;
	int m, n, d, j, i;
	digit uspace[5], vspace[5], qspace[5];

	/*
	 * Take care of special cases: divide by zero, and u < v.
	 */
	if (vq == 0) {
		/* divide by zero. */
		static volatile const unsigned int zero = 0;

		tmp.ul[H] = tmp.ul[L] = 1 / zero;
		if (arq) *arq = uq;
		return (tmp.q);
	}
	if (uq < vq) {
		if (arq) *arq = uq;
		return (0);
	}
	u = &uspace[0];
	v = &vspace[0];
	q = &qspace[0];

	/*
	 * Break dividend and divisor into digits in base B, then
	 * count leading zeros to determine m and n.  When done, we
	 * will have:
	 *	u = (u[1]u[2]...u[m+n]) sub B
	 *	v = (v[1]v[2]...v[n]) sub B
	 *	v[1] != 0
	 *	1 < n <= 4 (if n = 1, we use a different division algorithm)
	 *	m >= 0 (otherwise u < v, which we already checked)
	 *	m + n = 4
	 * and thus
	 *	m = 4 - n <= 2
	 */
	tmp.uq = uq;
	u[0] = 0;
	u[1] = HHALF(tmp.ul[H]);
	u[2] = LHALF(tmp.ul[H]);
	u[3] = HHALF(tmp.ul[L]);
	u[4] = LHALF(tmp.ul[L]);
	tmp.uq = vq;
	v[1] = HHALF(tmp.ul[H]);
	v[2] = LHALF(tmp.ul[H]);
	v[3] = HHALF(tmp.ul[L]);
	v[4] = LHALF(tmp.ul[L]);
	for (n = 4; v[1] == 0; v++) {
		if (--n == 1) {
			u32 rbj; /* r*B+u[j] (not root boy jim) */
			digit q1, q2, q3, q4;

			/*
			 * Change of plan, per exercise 16.
			 *	r = 0;
			 *	for j = 1..4:
			 *		q[j] = floor((r*B + u[j]) / v),
			 *		r = (r*B + u[j]) % v;
			 * We unroll this completely here.
			 */
			t = v[2]; /* nonzero, by definition */
			q1 = u[1] / t;
			rbj = COMBINE(u[1] % t, u[2]);
			q2 = rbj / t;
			rbj = COMBINE(rbj % t, u[3]);
			q3 = rbj / t;
			rbj = COMBINE(rbj % t, u[4]);
			q4 = rbj / t;
			if (arq) *arq = rbj % t;
			tmp.ul[H] = COMBINE(q1, q2);
			tmp.ul[L] = COMBINE(q3, q4);
			return (tmp.q);
		}
	}

	/*
	 * By adjusting q once we determine m, we can guarantee that
	 * there is a complete four-digit quotient at &qspace[1] when
	 * we finally stop.
	 */
	for (m = 4 - n; u[1] == 0; u++) {
		m--;
	}
	for (i = 4 - m; --i >= 0;) {
		q[i] = 0;
	}
	q += 4 - m;

	/*
	 * Here we run Program D, translated from MIX to C and acquiring
	 * a few minor changes.
	 *
	 * D1: choose multiplier 1 << d to ensure v[1] >= B/2.
	 */
	d = 0;
	for (t = v[1]; t < B / 2; t <<= 1) {
		d++;
	}
	if (d > 0) {
		__shl(&u[0], m + n, d); /* u <<= d */
		__shl(&v[1], n - 1, d); /* v <<= d */
	}
	/*
	 * D2: j = 0.
	 */
	j = 0;
	v1 = v[1]; /* for D3 -- note that v[1..n] are constant */
	v2 = v[2]; /* for D3 */
	do {
		register digit uj0, uj1, uj2;

		/*
		 * D3: Calculate qhat (\^q, in TeX notation).
		 * Let qhat = min((u[j]*B + u[j+1])/v[1], B-1), and
		 * let rhat = (u[j]*B + u[j+1]) mod v[1].
		 * While rhat < B and v[2]*qhat > rhat*B+u[j+2],
		 * decrement qhat and increase rhat correspondingly.
		 * Note that if rhat >= B, v[2]*qhat < rhat*B.
		 */
		uj0 = u[j + 0]; /* for D3 only -- note that u[j+...] change */
		uj1 = u[j + 1]; /* for D3 only */
		uj2 = u[j + 2]; /* for D3 only */
		if (uj0 == v1) {
			qhat = B;
			rhat = uj1;
			goto qhat_too_big;
		} else {
			u32 nn = COMBINE(uj0, uj1);
			qhat = nn / v1;
			rhat = nn % v1;
		}
		while (v2 * qhat > COMBINE(rhat, uj2)) {
qhat_too_big:
			qhat--;
			if ((rhat += v1) >= B) {
				break;
			}
		}
		/*
		 * D4: Multiply and subtract.
		 * The variable `t' holds any borrows across the loop.
		 * We split this up so that we do not require v[0] = 0,
		 * and to eliminate a final special case.
		 */
		for (t = 0, i = n; i > 0; i--) {
			t = u[i + j] - v[i] * qhat - t;
			u[i + j] = LHALF(t);
			t = (B - HHALF(t)) & (B - 1);
		}
		t = u[j] - t;
		u[j] = LHALF(t);
		/*
		 * D5: test remainder.
		 * There is a borrow if and only if HHALF(t) is nonzero;
		 * in that (rare) case, qhat was too large (by exactly 1).
		 * Fix it by adding v[1..n] to u[j..j+n].
		 */
		if (HHALF(t)) {
			qhat--;
			for (t = 0, i = n; i > 0; i--) { /* D6: add back. */
				t += u[i + j] + v[i];
				u[i + j] = LHALF(t);
				t = HHALF(t);
			}
			u[j] = LHALF(u[j] + t);
		}
		q[j] = qhat;
	} while (++j <= m); /* D7: loop on j. */

	/*
	 * If caller wants the remainder, we have to calculate it as
	 * u[m..m+n] >> d (this is at most n digits and thus fits in
	 * u[m+1..m+n], but we may need more source digits).
	 */
	if (arq) {
		if (d) {
			for (i = m + n; i > m; --i) {
				u[i] = (u[i] >> d) | LHALF(u[i - 1] << (HALF_BITS - d));
			}
			u[i] = 0;
		}
		tmp.ul[H] = COMBINE(uspace[1], uspace[2]);
		tmp.ul[L] = COMBINE(uspace[3], uspace[4]);
		*arq = tmp.q;
	}

	tmp.ul[H] = COMBINE(qspace[1], qspace[2]);
	tmp.ul[L] = COMBINE(qspace[3], qspace[4]);
	return (tmp.q);
}
