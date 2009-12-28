/*
 * /kernel/dev/video/vga.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/video/vga.h>

DefineDevFactory(VgaTerminal);

RegDevClass(VgaTerminal, "vga", Device::T_CHAR, "VGA text video terminal");

u8 VgaTerminal::defColTable[] = { 0, 4, 2, 6, 1, 5, 3, 7,
	8, 12, 10, 14, 9, 13, 11, 15 };

/* the default colour table, for VGA+ colour systems */
VgaTerminal::PalColor VgaTerminal::defPal[] = {
	{ 0x00, 0x00, 0x00 },
	{ 0xaa, 0x00, 0x00 },
	{ 0x00, 0xaa, 0x00 },
	{ 0xaa, 0x55, 0x00 },
	{ 0x00, 0x00, 0xaa },
	{ 0xaa, 0x00, 0xaa },
	{ 0x00, 0xaa, 0xaa },
	{ 0xaa, 0xaa, 0xaa },
	{ 0x55, 0x55, 0x55 },
	{ 0xff, 0x55, 0x55 },
	{ 0x55, 0xff, 0x55 },
	{ 0xff, 0xff, 0x55 },
	{ 0x55, 0x55, 0xff },
	{ 0xff, 0x55, 0xff },
	{ 0x55, 0xff, 0xff },
	{ 0xff, 0xff, 0xff },
};

VgaTerminal::VgaTerminal(Type type, u32 unit, u32 classID) : ChrDevice(type, unit, classID)
{
	/* only unit 0 is supported */
	if (unit) {
		return;
	}
	curPos = 0;
	if (Initialize()) {
		return;
	}
	devState = S_UP;
}

int
VgaTerminal::Initialize()
{
	u32 x = Lock();

	fb = (u16 *)mm->MapPhys(MEM_LOCATION, MEM_SIZE);
	if (!fb) {
		klog(KLOG_ERROR, "VGA frame buffer memory mapping failed");
		return -1;
	}
	/* check graphics card presence */
	fb[0] = 0x55aa;
	fb[1] = 0xaa55;
	if (fb[0] != 0x55aa || fb[1] != 0xaa55) {
		klog(KLOG_INFO, "VGA card not detected");
		return -1;
	}
	fb[0] = 0xaa55;
	fb[1] = 0x55aa;
	if (fb[0] != 0xaa55 || fb[1] != 0x55aa) {
		klog(KLOG_INFO, "VGA card not detected");
		return -1;
	}
	klog(KLOG_INFO, "VGA card detected");

	/*
	 * Normalize the palette registers, to point
	 * the 16 screen colors to the first 16
	 * DAC entries.
	 */
	for (int i = 0; i < 16; i++) {
		/* reset AR state by reading IS1 */
		inb(REG_IS1);
		outb(REG_AR_ADDR, i);
		outb(REG_AR_DATA_W, i);
	}
	outb(REG_AR_ADDR, REG_AR_PAS);

	/* install default palette */
	for (int i = 0; i < 16; i++) {
		outb(REG_DAC_ADDR_W, defColTable[i]);
		outb(REG_DAC_DATA, defPal[i].red);
		outb(REG_DAC_DATA, defPal[i].green);
		outb(REG_DAC_DATA, defPal[i].blue);
	}

	Resize(DEF_NUM_COLS, DEF_NUM_LINES);
	SetCursorSize(FONT_HEIGHT - 3, FONT_HEIGHT - 2);
	SetOrigin(0);
	SetCursor(0);
	curAttr = BuildAttr(COL_WHITE | COL_LIGHT, COL_BLACK);
	Unlock(x);
	return 0;
}

int
VgaTerminal::Resize(u32 cx, u32 cy)
{
	curCX = cx;
	curCY = cy;
	u32 scanLines = cy * FONT_HEIGHT;
	xRes = cx * FONT_WIDTH;
	yRes = scanLines;

	outb(REG_CRTC_ADDR, REG_CRTC_I_MSL);
	u8 maxScan = inb(REG_CRTC_DATA);
	if (maxScan & REG_CRTC_I_MSL_SD) {
		scanLines <<= 1;
	}

	outb(REG_CRTC_ADDR, REG_CRTC_I_MC);
	u8 mode = inb(REG_CRTC_DATA);
	if (mode & REG_CRTC_I_MC_SLDIV) {
		scanLines >>= 1;
	}

	scanLines--;
	outb(REG_CRTC_ADDR, REG_CRTC_I_OF);
	u8 r7 = inb(REG_CRTC_DATA) & ~(REG_CRTC_I_OF_VDE8 | REG_CRTC_I_OF_VDE9);
	if (scanLines & 0x100) {
		r7 |= REG_CRTC_I_OF_VDE8;
	}
	if (scanLines & 0x200) {
		r7 |= REG_CRTC_I_OF_VDE9;
	}

	/* unprotect registers */
	outb(REG_CRTC_ADDR, REG_CRTC_I_VRE);
	u8 vsyncEnd = inb(REG_CRTC_DATA);
	outb(REG_CRTC_ADDR, REG_CRTC_I_VRE);
	outb(REG_CRTC_DATA, vsyncEnd & ~REG_CRTC_I_VRE_PROT);

	outb(REG_CRTC_ADDR, REG_CRTC_I_EHD);
	outb(REG_CRTC_DATA, cx - 1);
	outb(REG_CRTC_ADDR, REG_CRTC_I_OFFS);
	outb(REG_CRTC_DATA, cx >> 1);

	outb(REG_CRTC_ADDR, REG_CRTC_I_VDE);
	outb(REG_CRTC_DATA, scanLines & 0xff);
	outb(REG_CRTC_ADDR, REG_CRTC_I_OF);
	outb(REG_CRTC_DATA, r7);

	/* protect registers */
	outb(REG_CRTC_ADDR, REG_CRTC_I_VRE);
	outb(REG_CRTC_DATA, vsyncEnd);
	return 0;
}

int
VgaTerminal::SetCursorSize(u32 from, u32 to)
{
	outb(REG_CRTC_ADDR, REG_CRTC_I_CS);
	u8 cs = inb(REG_CRTC_DATA);
	outb(REG_CRTC_ADDR, REG_CRTC_I_CE);
	u8 ce = inb(REG_CRTC_DATA);
	cs = (cs & ~REG_CRTC_I_CS_SLMASK) | (from & REG_CRTC_I_CS_SLMASK);
	ce = (ce & ~REG_CRTC_I_CE_SLMASK) | (to & REG_CRTC_I_CE_SLMASK);
	outb(REG_CRTC_ADDR, REG_CRTC_I_CS);
	outb(REG_CRTC_DATA, cs);
	outb(REG_CRTC_ADDR, REG_CRTC_I_CE);
	outb(REG_CRTC_DATA, ce);
	return 0;
}

int
VgaTerminal::SetOrigin(u32 org)
{
	outb(REG_CRTC_ADDR, REG_CRTC_I_SAH);
	outb(REG_CRTC_DATA, org >> 8);
	outb(REG_CRTC_ADDR, REG_CRTC_I_SAL);
	outb(REG_CRTC_DATA, org & 0xff);
	return 0;
}

int
VgaTerminal::SetCursor(u32 pos)
{
	outb(REG_CRTC_ADDR, REG_CRTC_I_CLH);
	outb(REG_CRTC_DATA, pos >> 8);
	outb(REG_CRTC_ADDR, REG_CRTC_I_CLL);
	outb(REG_CRTC_DATA, pos & 0xff);
	return 0;
}

u8
VgaTerminal::BuildAttr(u8 fgCol, u8 bgCol)
{
	return (fgCol & 0xf) | ((bgCol & 0xf) << 4);
}

u32
VgaTerminal::Lock()
{
	u32 rc = im->DisableIntr();
	lock.Lock();
	return rc;
}

void
VgaTerminal::Unlock(u32 x)
{
	lock.Unlock();
	im->RestoreIntr(x);
}

Device::IOStatus
VgaTerminal::Putc(u8 c)
{
	u16 data = c | ((u16)curAttr << 8);
	fb[curPos++] = data;
	return IOS_OK;
}
