/*
 * /kernel/lib/stdlib.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

ASMCALL void *
memset(void *dst, u8 fill, u32 size)
{
	void *ret = dst;
	while (size) {
		*(u8 *)dst = fill;
		dst = ((u8 *)dst) + 1;
		size--;
	}
	return ret;
}

ASMCALL void *
memcpy(void *dst, const void *src, u32 size)
{
	void *ret = dst;
	while (size) {
		*(u8 *)dst = *(u8 *)src;
		dst = ((u8 *)dst) + 1;
		src = ((u8 *)src) + 1;
		size--;
	}
	return ret;
}

ASMCALL void *
memmove(void *dst, const void *src, u32 size)
{
	void *ret = dst;
	if (src > dst) {
		while (size) {
			*(u8 *)dst = *(u8 *)src;
			dst = ((u8 *)dst) + 1;
			src = ((u8 *)src) + 1;
			size--;
		}
	} else {
		dst = (u8 *)dst + size - 1;
		src = (u8 *)src + size - 1;
		while (size) {
			*(u8 *)dst = *(u8 *)src;
			dst = ((u8 *)dst) - 1;
			src = ((u8 *)src) - 1;
			size--;
		}
	}
	return ret;
}

ASMCALL int
memcmp(const void *ptr1, const void *ptr2, u32 size)
{
	while (size) {
		if (*(u8 *)ptr1 != *(u8 *)ptr2) {
			return *(u8 *)ptr2 - *(u8 *)ptr1;
		}
	}
	return 0;
}

ASMCALL int
toupper(int c)
{
	if (c >= 'a' && c <= 'z') {
		c -= 'a' - 'A';
	}
	return c;
}

ASMCALL int
tolower(int c)
{
	if (c >= 'A' && c <= 'Z') {
		c += 'a' - 'A';
	}
	return c;
}

ASMCALL u32
strlen(const char *str)
{
	register const char *s;

	for (s = str; *s; ++s);
	return(s - str);
}

ASMCALL char *
strcpy(char *dst, const char *src)
{
	char *ret = dst;
	while (*src) {
		*dst = *src;
		dst++;
		src++;
	}
	return ret;
}

ASMCALL int
strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2) {
		if (!*s1) {
			return 0;
		}
		s1++;
		s2++;
	}
	return *s2 - *s1;
}

char *
strdup(const char *str)
{
	if (!str) {
		return 0;
	}
	u32 len = strlen(str) + 1;
	char *p = (char *)mm::malloc(len, 1);
	if (!p) {
		return 0;
	}
	memcpy(p, str, len);
	return p;
}

int
sprintf(char *buf, const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	return kvprintf(fmt, 0, buf, 10, va);
}

/* This is actually used with radix [2..36] */
char const hex2ascii_data[] = "0123456789abcdefghijklmnopqrstuvwxyz";

#define	hex2ascii(hex)	(hex2ascii_data[hex])

/*
 * Put a NUL-terminated ASCII number (base <= 36) in a buffer in reverse
 * order; return an optional length and a pointer to the last character
 * written in the buffer (i.e., the first character of the string).
 * The buffer pointed to by `nbuf' must have length >= MAXNBUF.
 */
static char *
ksprintn(char *nbuf, u64 num, int base, int *lenp, int upper)
{
	char *p, c;

	p = nbuf;
	*p = '\0';
	do {
		c = hex2ascii(num % base);
		*++p = upper ? toupper(c) : c;
	} while (num /= base);
	if (lenp) {
		*lenp = p - nbuf;
	}
	return p;
}

/* ported from FreeBSD */
int
kvprintf(const char *fmt, PutcFunc func, void *arg, int radix, va_list ap)
{
#define PCHAR(c) {int cc=(c); if (func) (*func)(cc, arg); else *d++ = cc; retval++;}
/* Max number conversion buffer length: a u_quad_t in base 2, plus NUL byte. */
#define MAXNBUF	(sizeof(i64) * NBBY + 1)
	char nbuf[MAXNBUF];
	char *d;
	const char *p, *percent, *q;
	u8 *up;
	int ch, n;
	u64 num;
	int base, lflag, qflag, tmp, width, ladjust, sharpflag, neg, sign, dot;
	int cflag, hflag, jflag, tflag, zflag;
	int dwidth, upper;
	char padc;
	int stop = 0, retval = 0;

	num = 0;
	if (!func) {
		d = (char *)arg;
	} else {
		d = 0;
	}

	if (fmt == 0) {
		fmt = "(fmt null)\n";
	}

	if (radix < 2 || radix > 36) {
		radix = 10;
	}

	for (;;) {
		padc = ' ';
		width = 0;
		while ((ch = (u8)*fmt++) != '%' || stop) {
			if (ch == '\0') {
				return (retval);
			}
			PCHAR(ch);
		}
		percent = fmt - 1;
		qflag = 0;
		lflag = 0;
		ladjust = 0;
		sharpflag = 0;
		neg = 0;
		sign = 0;
		dot = 0;
		dwidth = 0;
		upper = 0;
		cflag = 0;
		hflag = 0;
		jflag = 0;
		tflag = 0;
		zflag = 0;
reswitch:
		switch (ch = (u8)*fmt++) {
		case '.':
			dot = 1;
			goto reswitch;
		case '#':
			sharpflag = 1;
			goto reswitch;
		case '+':
			sign = 1;
			goto reswitch;
		case '-':
			ladjust = 1;
			goto reswitch;
		case '%':
			PCHAR(ch);
			break;
		case '*':
			if (!dot) {
				width = va_arg(ap, int);
				if (width < 0) {
					ladjust = !ladjust;
					width = -width;
				}
			} else {
				dwidth = va_arg(ap, int);
			}
			goto reswitch;
		case '0':
			if (!dot) {
				padc = '0';
				goto reswitch;
			}
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			for (n = 0;; ++fmt) {
				n = n * 10 + ch - '0';
				ch = *fmt;
				if (ch < '0' || ch > '9') {
					break;
				}
			}
			if (dot) {
				dwidth = n;
			} else {
				width = n;
			}
			goto reswitch;
		case 'b':
			num = (u32)va_arg(ap, int);
			p = va_arg(ap, char *);
			for (q = ksprintn(nbuf, num, *p++, 0, 0); *q;) {
				PCHAR(*q--);
			}
			if (num == 0) {
				break;
			}

			for (tmp = 0; *p;) {
				n = *p++;
				if (num & (1 << (n - 1))) {
					PCHAR(tmp ? ',' : '<');
					for (; (n = *p) > ' '; ++p) {
						PCHAR(n);
					}
					tmp = 1;
				} else
					for (; *p > ' '; ++p) {
						continue;
					}
			}
			if (tmp) {
				PCHAR('>');
			}
			break;
		case 'c':
			PCHAR(va_arg(ap, int));
			break;
		case 'D':
			up = va_arg(ap, u8 *);
			p = va_arg(ap, char *);
			if (!width) {
				width = 16;
			}
			while (width--) {
				PCHAR(hex2ascii(*up >> 4));
				PCHAR(hex2ascii(*up & 0x0f));
				up++;
				if (width) {
					for (q = p; *q; q++) {
						PCHAR(*q);
					}
				}
			}
			break;
		case 'd':
		case 'i':
			base = 10;
			sign = 1;
			goto handle_sign;
		case 'h':
			if (hflag) {
				hflag = 0;
				cflag = 1;
			} else {
				hflag = 1;
			}
			goto reswitch;
		case 'j':
			jflag = 1;
			goto reswitch;
		case 'l':
			if (lflag) {
				lflag = 0;
				qflag = 1;
			} else {
				lflag = 1;
			}
			goto reswitch;
		case 'n':
			if (jflag) *(va_arg(ap, i64 *)) = retval;
			else if (qflag) *(va_arg(ap, i64 *)) = retval;
			else if (lflag) *(va_arg(ap, long *)) = retval;
			else if (zflag) *(va_arg(ap, i32 *)) = retval;
			else if (hflag) *(va_arg(ap, short *)) = retval;
			else if (cflag) *(va_arg(ap, char *)) = retval;
			else *(va_arg(ap, int *)) = retval;
			break;
		case 'o':
			base = 8;
			goto handle_nosign;
		case 'p':
			base = 16;
			sharpflag = (width == 0);
			sign = 0;
			num = (u64)va_arg(ap, void *);
			goto number;
		case 'q':
			qflag = 1;
			goto reswitch;
		case 'r':
			base = radix;
			if (sign) {
				goto handle_sign;
			}
			goto handle_nosign;
		case 's':
			p = va_arg(ap, char *);
			if (p == 0) {
				p = "(null)";
			}
			if (!dot) {
				n = strlen((char *)p);
			} else {
				for (n = 0; n < dwidth && p[n]; n++) {
					continue;
				}
			}

			width -= n;

			if (!ladjust && width > 0) {
				while (width--) {
					PCHAR(padc);
				}
			}
			while (n--) {
				PCHAR(*p++);
			}
			if (ladjust && width > 0) {
				while (width--) {
					PCHAR(padc);
				}
			}
			break;
		case 't':
			tflag = 1;
			goto reswitch;
		case 'u':
			base = 10;
			goto handle_nosign;
		case 'X':
			upper = 1;
		case 'x':
			base = 16;
			goto handle_nosign;
		case 'y':
			base = 16;
			sign = 1;
			goto handle_sign;
		case 'z':
			zflag = 1;
			goto reswitch;
handle_nosign:
			sign = 0;
			if (jflag) num = va_arg(ap, u64);
			else if (qflag) num = va_arg(ap, u64);
			else if (tflag) num = va_arg(ap, i32);
			else if (lflag) num = va_arg(ap, u32);
			else if (zflag) num = va_arg(ap, u32);
			else if (hflag) num = (u16)va_arg(ap, int);
			else if (cflag) num = (u8)va_arg(ap, int);
			else num = va_arg(ap, u32);
			goto number;
handle_sign:
			if (jflag) num = va_arg(ap, i64);
			else if (qflag) num = va_arg(ap, i64);
			else if (tflag) num = va_arg(ap, i32);
			else if (lflag) num = va_arg(ap, long);
			else if (zflag) num = va_arg(ap, i32);
			else if (hflag) num = (short)va_arg(ap, int);
			else if (cflag) num = (char)va_arg(ap, int);
			else num = va_arg(ap, int);
number:
			if (sign && (i64)num < 0) {
				neg = 1;
				num = -(i64)num;
			}
			p = ksprintn(nbuf, num, base, &tmp, upper);
			if (sharpflag && num != 0) {
				if (base == 8) {
					tmp++;
				} else if (base == 16) {
					tmp += 2;
				}
			}
			if (neg) {
				tmp++;
			}

			if (!ladjust && padc != '0' && width && (width -= tmp) > 0) {
				while (width--) {
					PCHAR(padc);
				}
			}
			if (neg) {
				PCHAR('-');
			}
			if (sharpflag && num != 0) {
				if (base == 8) {
					PCHAR('0');
				} else if (base == 16) {
					PCHAR('0');
					PCHAR('x');
				}
			}
			if (!ladjust && width && (width -= tmp) > 0) {
				while (width--) {
					PCHAR(padc);
				}
			}

			while (*p) {
				PCHAR(*p--);
			}

			if (ladjust && width && (width -= tmp) > 0) {
				while (width--) {
					PCHAR(padc);
				}
			}

			break;
		default:
			while (percent < fmt) {
				PCHAR(*percent++);
			}
			/*
			 * Since we ignore an formatting argument it is no
			 * longer safe to obey the remaining formatting
			 * arguments as the arguments will no longer match
			 * the format specs.
			 */
			stop = 1;
			break;
		}
	}
#undef PCHAR
}

u32 hashtable[256] = {
	0xc76a29e1UL, 0x77073096UL, 0xee0e612cUL, 0x990951baUL, 0x076dc419UL,
	0x706af48fUL, 0xe963a535UL, 0x9e6495a3UL, 0x0edb8832UL, 0x79dcb8a4UL,
	0xe0d5e91eUL, 0x97d2d988UL, 0x09b64c2bUL, 0x7eb17cbdUL, 0xe7b82d07UL,
	0x90bf1d91UL, 0x1db71064UL, 0x6ab020f2UL, 0xf3b97148UL, 0x84be41deUL,
	0x1adad47dUL, 0x6ddde4ebUL, 0xf4d4b551UL, 0x83d385c7UL, 0x136c9856UL,
	0x646ba8c0UL, 0xfd62f97aUL, 0x8a65c9ecUL, 0x14015c4fUL, 0x63066cd9UL,
	0xfa0f3d63UL, 0x8d080df5UL, 0x3b6e20c8UL, 0x4c69105eUL, 0xd56041e4UL,
	0xa2677172UL, 0x3c03e4d1UL, 0x4b04d447UL, 0xd20d85fdUL, 0xa50ab56bUL,
	0x35b5a8faUL, 0x42b2986cUL, 0xdbbbc9d6UL, 0xacbcf940UL, 0x32d86ce3UL,
	0x45df5c75UL, 0xdcd60dcfUL, 0xabd13d59UL, 0x26d930acUL, 0x51de003aUL,
	0xc8d75180UL, 0xbfd06116UL, 0x21b4f4b5UL, 0x56b3c423UL, 0xcfba9599UL,
	0xb8bda50fUL, 0x2802b89eUL, 0x5f058808UL, 0xc60cd9b2UL, 0xb10be924UL,
	0x2f6f7c87UL, 0x58684c11UL, 0xc1611dabUL, 0xb6662d3dUL, 0x76dc4190UL,
	0x01db7106UL, 0x98d220bcUL, 0xefd5102aUL, 0x71b18589UL, 0x06b6b51fUL,
	0x9fbfe4a5UL, 0xe8b8d433UL, 0x7807c9a2UL, 0x0f00f934UL, 0x9609a88eUL,
	0xe10e9818UL, 0x7f6a0dbbUL, 0x086d3d2dUL, 0x91646c97UL, 0xe6635c01UL,
	0x6b6b51f4UL, 0x1c6c6162UL, 0x856530d8UL, 0xf262004eUL, 0x6c0695edUL,
	0x1b01a57bUL, 0x8208f4c1UL, 0xf50fc457UL, 0x65b0d9c6UL, 0x12b7e950UL,
	0x8bbeb8eaUL, 0xfcb9887cUL, 0x62dd1ddfUL, 0x15da2d49UL, 0x8cd37cf3UL,
	0xfbd44c65UL, 0x4db26158UL, 0x3ab551ceUL, 0xa3bc0074UL, 0xd4bb30e2UL,
	0x4adfa541UL, 0x3dd895d7UL, 0xa4d1c46dUL, 0xd3d6f4fbUL, 0x4369e96aUL,
	0x346ed9fcUL, 0xad678846UL, 0xda60b8d0UL, 0x44042d73UL, 0x33031de5UL,
	0xaa0a4c5fUL, 0xdd0d7cc9UL, 0x5005713cUL, 0x270241aaUL, 0xbe0b1010UL,
	0xc90c2086UL, 0x5768b525UL, 0x206f85b3UL, 0xb966d409UL, 0xce61e49fUL,
	0x5edef90eUL, 0x29d9c998UL, 0xb0d09822UL, 0xc7d7a8b4UL, 0x59b33d17UL,
	0x2eb40d81UL, 0xb7bd5c3bUL, 0xc0ba6cadUL, 0xedb88320UL, 0x9abfb3b6UL,
	0x03b6e20cUL, 0x74b1d29aUL, 0xead54739UL, 0x9dd277afUL, 0x04db2615UL,
	0x73dc1683UL, 0xe3630b12UL, 0x94643b84UL, 0x0d6d6a3eUL, 0x7a6a5aa8UL,
	0xe40ecf0bUL, 0x9309ff9dUL, 0x0a00ae27UL, 0x7d079eb1UL, 0xf00f9344UL,
	0x8708a3d2UL, 0x1e01f268UL, 0x6906c2feUL, 0xf762575dUL, 0x806567cbUL,
	0x196c3671UL, 0x6e6b06e7UL, 0xfed41b76UL, 0x89d32be0UL, 0x10da7a5aUL,
	0x67dd4accUL, 0xf9b9df6fUL, 0x8ebeeff9UL, 0x17b7be43UL, 0x60b08ed5UL,
	0xd6d6a3e8UL, 0xa1d1937eUL, 0x38d8c2c4UL, 0x4fdff252UL, 0xd1bb67f1UL,
	0xa6bc5767UL, 0x3fb506ddUL, 0x48b2364bUL, 0xd80d2bdaUL, 0xaf0a1b4cUL,
	0x36034af6UL, 0x41047a60UL, 0xdf60efc3UL, 0xa867df55UL, 0x316e8eefUL,
	0x4669be79UL, 0xcb61b38cUL, 0xbc66831aUL, 0x256fd2a0UL, 0x5268e236UL,
	0xcc0c7795UL, 0xbb0b4703UL, 0x220216b9UL, 0x5505262fUL, 0xc5ba3bbeUL,
	0xb2bd0b28UL, 0x2bb45a92UL, 0x5cb36a04UL, 0xc2d7ffa7UL, 0xb5d0cf31UL,
	0x2cd99e8bUL, 0x5bdeae1dUL, 0x9b64c2b0UL, 0xec63f226UL, 0x756aa39cUL,
	0x026d930aUL, 0x9c0906a9UL, 0xeb0e363fUL, 0x72076785UL, 0x05005713UL,
	0x95bf4a82UL, 0xe2b87a14UL, 0x7bb12baeUL, 0x0cb61b38UL, 0x92d28e9bUL,
	0xe5d5be0dUL, 0x7cdcefb7UL, 0x0bdbdf21UL, 0x86d3d2d4UL, 0xf1d4e242UL,
	0x68ddb3f8UL, 0x1fda836eUL, 0x81be16cdUL, 0xf6b9265bUL, 0x6fb077e1UL,
	0x18b74777UL, 0x88085ae6UL, 0xff0f6a70UL, 0x66063bcaUL, 0x11010b5cUL,
	0x8f659effUL, 0xf862ae69UL, 0x616bffd3UL, 0x166ccf45UL, 0xa00ae278UL,
	0xd70dd2eeUL, 0x4e048354UL, 0x3903b3c2UL, 0xa7672661UL, 0xd06016f7UL,
	0x4969474dUL, 0x3e6e77dbUL, 0xaed16a4aUL, 0xd9d65adcUL, 0x40df0b66UL,
	0x37d83bf0UL, 0xa9bcae53UL, 0xdebb9ec5UL, 0x47b2cf7fUL, 0x30b5ffe9UL,
	0xbdbdf21cUL, 0xcabac28aUL, 0x53b39330UL, 0x24b4a3a6UL, 0xbad03605UL,
	0xcdd70693UL, 0x54de5729UL, 0x23d967bfUL, 0xb3667a2eUL, 0xc4614ab8UL,
	0x5d681b02UL, 0x2a6f2b94UL, 0xb40bbe37UL, 0xc30c8ea1UL, 0x5a05df1bUL,
	0x2d02ef8dUL
};

u32
gethash(const char *s)
{
	u32 hash = 0x5a5a5a5a;
	while (*s) {
		u8 c = *(u8 *)s++;
		hash += hashtable[c];
		hash ^= hashtable[(c * (c + 13)) & 0xff] +
			(c + 1) * (c + 17) * (c + 31) * (c + 41);
	}
	return hash;
}

u32 gethash(u8 *data, u32 size)
{
	u32 hash = 0x5a5a5a5a;
	while (size) {
		u8 c = *data++;
		hash += hashtable[c];
		hash ^= hashtable[(c * (c + 13)) & 0xff] +
			(c + 1) * (c + 17) * (c + 31) * (c + 41);
		size--;
	}
	return hash;
}

u64 gethash64(const char *s)
{
	u64 hash = 0x5a5a5a5aa5a5a5a5ull;
	while (*s) {
		u8 c = *(u8 *)s++;
		hash += (u64)hashtable[c] | ((u64)hashtable[c * (c + 5)] << 32);
		hash ^= (u64)hashtable[(c * (c + 13)) & 0xff] +
			((u64)hashtable[(c * (c + 17)) & 0xff] << 32) +
			(c + 1) * (c + 17) * (c + 31) * (c + 41) *
			(c + 89) * (c + 113) * (c + 181) * (c + 277);
	}
	return hash;
}

u64
gethash64(u8 *data, u32 size)
{
	u64 hash = 0x5a5a5a5aa5a5a5a5ull;
	while (size) {
		u8 c = *data++;
		hash += (u64)hashtable[c] | ((u64)hashtable[c * (c + 5)] << 32);
		hash ^= (u64)hashtable[(c * (c + 13)) & 0xff] +
			((u64)hashtable[(c * (c + 17)) & 0xff] << 32) +
			(c + 1) * (c + 17) * (c + 31) * (c + 41) *
			(c + 89) * (c + 113) * (c + 181) * (c + 277);
		size--;
	}
	return hash;
}

/* XXX should be moved */
void klog(KLogLevel level, const char *fmt,...)
{
	static KLogLevel curLevel = KLOG_DEBUG;
	static const char *levelStr[] = {"DEBUG", "INFO", "WARNING", "ERROR"};

	if (level >= curLevel) {
		va_list va;
		va_start(va, fmt);
		printf("[%s] kernel: ", levelStr[level]);
		vprintf(fmt, va);
		printf("\n");
	}
}
