/*
 * /kernel/dev/video/vga.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef VGA_H_
#define VGA_H_
#include <sys.h>
phbSource("$Id$");

class VgaTerminal : public ConsoleDev {
private:
	enum Regs {
		REG_MO_W =			0x3cc, /* Miscellaneous Output Register */
		REG_MO_R =			0x3c2,
		REG_MO_F_VSYNCP =	0x80, /* Vertical Sync Polarity */
		REG_MO_F_HSYNCP =	0x40, /* Horizontal Sync Polarity */
		REG_MO_F_OEP =		0x20, /* Odd/Even Page Select */
		REG_MO_F_CSMASK =	0x0c, /* Clock Select bits */
		REG_MO_F_CS25 =		0x00, /* 25 MHz (320x640) */
		REG_MO_F_CS28 =		0x04, /* 28 MHz (360x720) */
		REG_MO_F_RAME =		0x02, /* RAM Enable */
		REG_MO_F_IOAS =		0x01, /* Input/Output Address Select */

		REG_CRTC_ADDR =		0x3d4, /* CRT Controller registers */
		REG_CRTC_DATA =		0x3d5,
		REG_CRTC_I_HT =		0x00, /* Horizontal Total Register */
		REG_CRTC_I_EHD =	0x01, /* End Horizontal Display Register */
		REG_CRTC_I_SHB =	0x02, /* Start Horizontal Blanking Register */
		REG_CRTC_I_EHB =	0x03, /* End Horizontal Blanking Register */
		REG_CRTC_I_SHR =	0x04, /* Start Horizontal Retrace Register */
		REG_CRTC_I_EHR =	0x05, /* End Horizontal Retrace Register */
		REG_CRTC_I_VT =		0x06, /* Vertical Total Register */

		REG_CRTC_I_OF =		0x07, /* Overflow Register */
		REG_CRTC_I_OF_VT8 =	0x01, /* Vertical Total (bit 8) */
		REG_CRTC_I_OF_VDE8 = 0x02, /* Vertical Display End (bit 8) */
		REG_CRTC_I_OF_VRS8 = 0x04, /* Vertical Retrace Start (bit 8) */
		REG_CRTC_I_OF_SVB8 = 0x08, /* Start Vertical Blanking (bit 8) */
		REG_CRTC_I_OF_LC8 =	0x10, /* Line Compare (bit 8) */
		REG_CRTC_I_OF_VT9 =	0x20, /* Vertical Total (bit 9) */
		REG_CRTC_I_OF_VDE9 = 0x40, /* Vertical Display End (bit9) */
		REG_CRTC_I_OF_VRS9 = 0x80, /* Vertical Retrace Start (bit 9) */

		REG_CRTC_I_PRS =	0x08, /* Preset Row Scan Register */

		REG_CRTC_I_MSL =	0x09, /* Maximum Scan Line Register */
		REG_CRTC_I_MSL_SVB9 = 0x20, /*  Start Vertical Blanking (bit 9) */
		REG_CRTC_I_MSL_LC9 = 0x40,	/* Line Compare (bit 9) */
		REG_CRTC_I_MSL_SD =	0x80, /* Scan Doubling */

		REG_CRTC_I_CS =		0x0a, /* Cursor Start Register */
		REG_CRTC_I_CS_SLMASK = 0x1f, /* Scan Line Mask */
		REG_CRTC_I_CS_CD =	0x20, /* Cursor Disable */

		REG_CRTC_I_CE =		0x0b, /* Cursor End Register */
		REG_CRTC_I_CE_SLMASK = 0x1f, /* Scan Line Mask */
		REG_CRTC_I_CE_CSK =	0x60, /* Cursor Skew */

		REG_CRTC_I_SAH =	0x0c, /* Start Address High Register */
		REG_CRTC_I_SAL =	0x0d, /* Start Address Low Register */
		REG_CRTC_I_CLH =	0x0e, /* Cursor Location High Register */
		REG_CRTC_I_CLL =	0x0f, /* Cursor Location Low Register */
		REG_CRTC_I_VRS =	0x10, /* Vertical Retrace Start Register */

		REG_CRTC_I_VRE =	0x11, /* Vertical Retrace End Register */
		REG_CRTC_I_VRE_MASK = 0x0f, /* Vertical Retrace End Mask */
		REG_CRTC_I_VRE_BW =	0x40, /* Memory Refresh Bandwidth */
		REG_CRTC_I_VRE_PROT = 0x80, /* CRTC Registers Protect Enable */

		REG_CRTC_I_VDE =	0x12, /* Vertical Display End Register */
		REG_CRTC_I_OFFS =	0x13, /* Offset Register */
		REG_CRTC_I_UL =		0x14, /* Underline Location Register */
		REG_CRTC_I_SVB =	0x15, /* Start Vertical Blanking Register */
		REG_CRTC_I_EVB=		0x16, /* End Vertical Blanking Register */

		REG_CRTC_I_MC =		0x17, /* CRTC Mode Control Register */
		REG_CRTC_I_MC_MAP13 = 0x01, /* Map Display Address 13 */
		REG_CRTC_I_MC_MAP14 = 0x02, /* Map Display Address 14 */
		REG_CRTC_I_MC_SLDIV = 0x04, /* Divide Scan Line clock by 2 */
		REG_CRTC_I_MC_DIV2 = 0x08, /* Divide Memory Address clock by 2 */
		REG_CRTC_I_MC_AW =	0x20, /* Address Wrap Select */
		REG_CRTC_I_MC_WB =	0x40, /* Word/Byte -- Word/Byte Mode Select */
		REG_CRTC_I_MC_SE =	0x80, /* Sync Enable */

		REG_CRTC_I_LC =		0x18, /* Line Compare Register */

		REG_GR_ADDR =		0x3ce, /* Graphics Registers */
		REG_GR_DATA =		0x3cf,
		REG_GR_I_SR =		0x00, /* Set/Reset Register */
		REG_GR_I_ESR =		0x01, /* Enable Set/Reset Register */
		REG_GR_I_CC =		0x02, /* Color Compare Register */
		REG_GR_I_DR =		0x03, /* Data Rotate Register */
		REG_GR_I_RMS =		0x04, /* Read Map Select Register */
		REG_GR_I_GM =		0x05, /* Graphics Mode Register */
		REG_GR_I_MG =		0x06, /* Miscellaneous Graphics Register */
		REG_GR_I_CDC =		0x07, /* Color Don't Care Register */
		REG_GR_I_BM =		0x08, /* Bit Mask Register */

		REG_IS0 =			0x3ca, /* Input Status #0 Register */
		REG_IS0_SS =		0x10, /* Switch Sense */

		REG_IS1 =			0x3da, /* Input Status #1 Register */
		REG_IS1_DD =		0x01, /* Display Disabled */
		REG_IS1_VR =		0x08, /* Vertical Retrace */

		REG_AR_ADDR =		0x3c0, /* Attribute Address Register */
		REG_AR_DATA_W =		0x3c0, /* Attribute Address Register */
		REG_AR_ADDRMASK =	0x1f, /* Attribute Address Mask */
		REG_AR_PAS =		0x20, /* Palette Address Source */
		REG_AR_DATA_R =		0x3c1, /* Attribute Data Read Register */
		REG_AR_I_AMC =		0x10, /* Attribute Mode Control Register */
		REG_AR_I_AMC_ATGE =	0x01, /* Attribute Controller Graphics Enable */
		REG_AR_I_AMC_MONO =	0x02, /* Monochrome Emulation */
		REG_AR_I_AMC_LGE =	0x04, /* Line Graphics Enable */
		REG_AR_I_AMC_BLINK =	0x08, /* Blink Enable */
		REG_AR_I_AMC_PPM =	0x20, /* Pixel Panning Mode */
		REG_AR_I_AMC_8BIT =	0x40, /* 8-bit Color Enable */
		REG_AR_I_AMC_P54S =	0x80, /* Palette Bits 5-4 Select */

		REG_AR_I_OC =		0x11, /* Overscan Color Register */

		REG_AR_I_CPE =		0x12, /* Color Plane Enable Register */
		REG_AR_I_CPE_P0 =	0x1, /* Plane 0 */
		REG_AR_I_CPE_P1 =	0x2, /* Plane 1 */
		REG_AR_I_CPE_P2 =	0x4, /* Plane 2 */
		REG_AR_I_CPE_P3 =	0x8, /* Plane 3 */

		REG_AR_I_HPP =		0x13, /* Horizontal Pixel Panning Register */

		REG_AR_I_CS =		0x14, /* Color Select Register */
		REG_AR_I_CS_54 =	0x03, /* Color Select 5-4 */
		REG_AR_I_CS_76 =	0x0c, /* Color Select 7-6 */

		REG_DAC_ADDR_W =	0x3c8, /* DAC Address Write Mode Register */
		REG_DAC_ADDR_R =	0x3c7, /* DAC Address Read Mode Register */
		REG_DAC_DATA =		0x3c9, /* DAC Data Register */
		REG_DAC_STATE =		0x3c7, /* DAC State Register */
	};

	enum {
		MEM_LOCATION =		0xb8000,
		MEM_SIZE =			0x8000,
		FONT_WIDTH =		8,
		FONT_HEIGHT =		16,
		DEF_NUM_COLS =		80,
		DEF_NUM_LINES =		40,
		DEF_FG_COLOR =		COL_WHITE | COL_BRIGHT,
		DEF_BG_COLOR =		COL_BLACK,
	};

	SpinLock lock;

	static u8 defColTable[16];
	typedef struct {
		int red, green, blue;
	} PalColor;
	static PalColor defPal[16];
	u16 *fb;
	u32 cx, cy, xRes, yRes;
	u8 curAttr;
	u32 curPos, curTop;

	int Initialize();
	u32 Lock();
	void Unlock(u32 x);
	int Resize(u32 cx, u32 cy); /* must be called locked */
	int SetCursorSize(u32 from, u32 to); /* must be called locked */
	int SetOrigin(u32 org); /* must be called locked */
	int SetCursor(u32 pos); /* must be called locked */
	u8 BuildAttr(u8 fgCol, u8 bgCol);
public:
	DeclareDevFactory();
	VgaTerminal(Type type, u32 unit, u32 classID);

	virtual IOStatus Putc(u8 c);
	virtual int SetFgColor(int col); /* return previous value */
	virtual int SetBgColor(int col); /* return previous value */
	virtual int Clear();
};

#endif /* VGA_H_ */
