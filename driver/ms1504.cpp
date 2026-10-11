// license:BSD-3-Clause
// copyright-holders:ajfa
/***************************************************************************

    Elektronika MS 1504 (PK-300), NPO Integral, Minsk, 1991

    Soviet PC/XT compatible laptop, modelled on the Toshiba T1100 Plus.
    KR1834VM86 CPU at 4.77/7.16 MHz, 640 KB RAM, 640x200 CGA compatible LCD,
    two 3.5" 720 KB drives, MC146818 compatible RTC at 70h/71h.

    BIOS "MINSK.....28/06.90 PC300", 32 KB, mirrored twice in a 27C512;
    a later "MINSK 18/02/93 XT310144P" adds XTA hard disk support.

    I/O beyond a plain XT, as used by the BIOS:
    - 070h/071h  MC146818 compatible RTC (KA512VI1)
    - 0E0h       clock: bit 7 set = 4.77 MHz, clear = 7.16 MHz (Ctrl+Alt+PgDn/PgUp)
    - 3DFh       bit 0 maps the 4 KB character generator RAM at B800:0000
    - 3F7h       AT style DIR; bits 5/6 report an external 5.25" drive

    The character RAM holds two 8x8 fonts, thin at 000h and thick at 800h. The
    BIOS can load three upper halves (INT 10h AX=1102h, BL=0..2, or Rus/Lat
    held + F1..F3): GOST "basic" Cyrillic, CP866 and CP437.

    Not emulated: XTA hard disk, the IOLA network adapter, external 5.25"
    drive, LCD contrast and backlight.

***************************************************************************/

#include "emu.h"

#include "bus/isa/cga.h"
#include "bus/isa/com.h"
#include "bus/isa/fdc.h"
#include "bus/isa/lpt.h"
#include "bus/pc_kbd/pc_kbdc.h"
#include "cpu/i86/i86.h"
#include "machine/genpc.h"
#include "machine/mc146818.h"
#include "machine/ram.h"

#define LOG_LCDC  (1U << 1)
#define LOG_PORTS (1U << 2)
#define LOG_KBD   (1U << 3)

#define VERBOSE (0)
#include "logmacro.h"


namespace {

class ms1504_lcdc_device : public isa8_cga_device
{
public:
	ms1504_lcdc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	virtual uint8_t io_read(offs_t offset) override;
	virtual void io_write(offs_t offset, uint8_t data) override;
	virtual MC6845_UPDATE_ROW( crtc_update_row ) override;

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

private:
	static constexpr rgb_t LCD_OFF = rgb_t(78, 100, 92);
	static constexpr rgb_t LCD_ON = rgb_t(190, 205, 170);

	void remap_vram();
	rgb_t lcd_shade(int level) const;

	uint8_t m_ctrl;
};

ROM_START( ms1504_lcdc )
	ROM_REGION(0x2000, "gfx1", ROMREGION_ERASE00)
ROM_END

INPUT_PORTS_START( ms1504_lcdc )
	PORT_START("cga_config")
	PORT_CONFNAME( 0x03, 0x00, "Monitor character set")
	PORT_CONFSETTING(    0x00, "Thick")
	PORT_CONFSETTING(    0x01, "Thin")
	PORT_BIT( 0x1c, 0x00, IPT_UNUSED )
	PORT_BIT( 0xe0, 0x00, IPT_UNUSED )
	PORT_CONFNAME( 0x100, 0x100, "Display")
	PORT_CONFSETTING(     0x100, "LCD")
	PORT_CONFSETTING(     0x000, "External RGB monitor")
	PORT_CONFNAME( 0x200, 0x200, "LCD intensity")
	PORT_CONFSETTING(     0x200, "Thick font")
	PORT_CONFSETTING(     0x000, "Ignored")
INPUT_PORTS_END

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(MS1504_LCDC, isa8_cga_device, ms1504_lcdc_device, "ms1504_lcdc", "MS 1504 LCD controller")

namespace {

ms1504_lcdc_device::ms1504_lcdc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: isa8_cga_device(mconfig, MS1504_LCDC, tag, owner, clock)
	, m_ctrl(0)
{
	m_chr_gen_offset[0] = m_chr_gen_offset[2] = 0x800;
	m_chr_gen_offset[1] = m_chr_gen_offset[3] = 0x000;
}

const tiny_rom_entry *ms1504_lcdc_device::device_rom_region() const
{
	return ROM_NAME(ms1504_lcdc);
}

ioport_constructor ms1504_lcdc_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(ms1504_lcdc);
}

void ms1504_lcdc_device::device_add_mconfig(machine_config &config)
{
	isa8_cga_device::device_add_mconfig(config);
	// 640x200 panel, proportions measured on a photo of the machine
	m_screen.lookup()->set_physical_aspect(19, 10);
}

rgb_t ms1504_lcdc_device::lcd_shade(int level) const
{
	return rgb_t(
			LCD_OFF.r() + (LCD_ON.r() - LCD_OFF.r()) * level / 3,
			LCD_OFF.g() + (LCD_ON.g() - LCD_OFF.g()) * level / 3,
			LCD_OFF.b() + (LCD_ON.b() - LCD_OFF.b()) * level / 3);
}

MC6845_UPDATE_ROW( ms1504_lcdc_device::crtc_update_row )
{
	uint32_t const cfg = m_cga_config->read();
	if (!BIT(cfg, 8))
	{
		isa8_cga_device::crtc_update_row(bitmap, cliprect, ma, ra, y, x_count, cursor_x, de, hbp, vbp);
		return;
	}
	if (m_update_row_type == -1)
		return;

	y = m_y;
	if (y >= bitmap.height())
		return;

	uint8_t const *const vram = &m_vram[m_start_offset];
	uint32_t *p = &bitmap.pix(y);

	if (BIT(m_mode_control, 1))
	{
		uint16_t const odd = (y & 1) << 13;
		if (BIT(m_mode_control, 4))
		{
			for (int i = 0; i < x_count * 2; i++)
			{
				uint8_t const data = vram[(((ma << 1) + i) & 0x1fff) | odd];
				for (int b = 7; b >= 0; b--)
					*p++ = BIT(data, b) ? LCD_ON : LCD_OFF;
			}
		}
		else
		{
			rgb_t shades[4];
			rgb_t const *const pal = m_palette->palette()->entry_list_raw();
			for (int c = 0; c < 4; c++)
			{
				rgb_t const col = pal[m_palette_lut_2bpp[c]];
				int const lum = (col.r() * 30 + col.g() * 59 + col.b() * 11) / 100;
				shades[c] = lcd_shade((lum * 3 + 127) / 255);
			}
			for (int i = 0; i < x_count * 2; i++)
			{
				uint8_t const data = vram[(((ma << 1) + i) & 0x1fff) | odd];
				for (int b = 6; b >= 0; b -= 2)
				{
					rgb_t const s = shades[(data >> b) & 3];
					*p++ = s;
					*p++ = s;
				}
			}
		}
		return;
	}

	bool const blink = BIT(m_mode_control, 5);
	bool const bold = BIT(cfg, 9);
	for (int i = 0; i < x_count; i++)
	{
		uint16_t const offset = ((ma + i) << 1) & 0x3fff;
		uint8_t const chr = vram[offset];
		uint8_t const attr = vram[offset + 1];
		uint8_t const *const font = m_chr_gen_base + ((bold && BIT(attr, 3)) ? 0x800 : 0x000);
		uint8_t data = font[chr * 8 + ra];
		bool fg = (attr & 0x0f) != 0;
		bool bg = ((attr >> 4) & 0x07) != 0 || (!blink && BIT(attr, 7));

		if ((i == cursor_x) && (m_framecnt & 0x08))
			data = 0xff;
		else if (blink && BIT(attr, 7) && (m_framecnt & 0x10))
			data = 0x00;

		for (int b = 7; b >= 0; b--)
			*p++ = (BIT(data, b) ? fg : bg) ? LCD_ON : LCD_OFF;
	}
}

void ms1504_lcdc_device::device_start()
{
	isa8_cga_device::device_start();
	m_isa->install_device(0x3d0, 0x3df,
			read8sm_delegate(*this, FUNC(ms1504_lcdc_device::io_read)),
			write8sm_delegate(*this, FUNC(ms1504_lcdc_device::io_write)));
	save_item(NAME(m_ctrl));
}

void ms1504_lcdc_device::device_reset()
{
	isa8_cga_device::device_reset();
	m_ctrl = 0;
	remap_vram();
}

void ms1504_lcdc_device::remap_vram()
{
	if (BIT(m_ctrl, 0))
	{
		m_isa->install_bank(0xb8000, 0xb8fff, m_chr_gen_base);
	}
	else
	{
		m_isa->install_bank(0xb8000, 0xbbfff, &m_vram[0]);
		m_isa->install_bank(0xbc000, 0xbffff, &m_vram[0]);
	}
}

uint8_t ms1504_lcdc_device::io_read(offs_t offset)
{
	if (offset == 0x0f)
		return m_ctrl;
	return isa8_cga_device::io_read(offset);
}

void ms1504_lcdc_device::io_write(offs_t offset, uint8_t data)
{
	if (offset == 0x0f)
	{
		LOGMASKED(LOG_LCDC, "%s: 3DF <- %02x\n", machine().describe_context(), data);
		m_ctrl = data;
		remap_vram();
		return;
	}
	if (offset == 0x09 || offset == 0x08)
		LOGMASKED(LOG_LCDC, "%s: 3D%X <- %02x\n", machine().describe_context(), offset, data);
	isa8_cga_device::io_write(offset, data);
}

/*
    Keyboard: KR1835VE49 (80C49) with an undumped mask ROM, so this is HLE.
    It speaks the XT protocol with XT scan codes plus 54h SysReq, 57h/58h F11/F12,
    5Ah Rus/Lat (held) and 5Bh Rus/Lat (lock). Legends follow the BIOS table at
    F000:A25E in Latin mode: the top row gives punctuation unshifted and digits
    with Shift, and the extra symbols while Rus/Lat is held.
*/

class ms1504_keyboard_device : public device_t, public device_pc_kbd_interface
{
public:
	ms1504_keyboard_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;

	virtual void clock_write(int state) override;
	virtual void data_write(int state) override;

private:
	static constexpr int ROWS = 12;

	TIMER_CALLBACK_MEMBER(scan_tick);
	TIMER_CALLBACK_MEMBER(bit_tick);

	void queue(uint8_t code);
	void try_send();

	required_ioport_array<ROWS> m_rows;
	emu_timer *m_scan_timer;
	emu_timer *m_bit_timer;

	uint8_t m_state[ROWS];
	uint8_t m_fifo[16];
	int m_fifo_head;
	int m_fifo_count;
	uint16_t m_shift;
	int m_bits;
	int m_phase;
	int m_kbclk;
	bool m_host_low;
	attotime m_host_low_since;
	uint8_t m_repeat_code;
	int m_repeat_ticks;
};

#define MS1504_KEY(mask, name, code, ...) \
	PORT_BIT(mask, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(name) PORT_CODE(code) __VA_ARGS__

INPUT_PORTS_START( ms1504_keyboard )
	PORT_START("ROW0")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_UNUSED)
	MS1504_KEY(0x02, "Esc", KEYCODE_ESC, PORT_CHAR(UCHAR_MAMEKEY(ESC)))
	MS1504_KEY(0x04, ")  1", KEYCODE_1, PORT_CHAR(')') PORT_CHAR('1') PORT_CHAR('&'))
	MS1504_KEY(0x08, "-  2", KEYCODE_2, PORT_CHAR('-') PORT_CHAR('2') PORT_CHAR('@'))
	MS1504_KEY(0x10, "/  3", KEYCODE_3, PORT_CHAR('/') PORT_CHAR('3') PORT_CHAR('^'))
	MS1504_KEY(0x20, "\"  4", KEYCODE_4, PORT_CHAR('"') PORT_CHAR('4') PORT_CHAR('{'))
	MS1504_KEY(0x40, ":  5", KEYCODE_5, PORT_CHAR(':') PORT_CHAR('5') PORT_CHAR('}'))
	MS1504_KEY(0x80, ",  6", KEYCODE_6, PORT_CHAR(',') PORT_CHAR('6') PORT_CHAR('\\'))

	PORT_START("ROW1")
	MS1504_KEY(0x01, ".  7", KEYCODE_7, PORT_CHAR('.') PORT_CHAR('7') PORT_CHAR('<'))
	MS1504_KEY(0x02, "_  8", KEYCODE_8, PORT_CHAR('_') PORT_CHAR('8') PORT_CHAR('>'))
	MS1504_KEY(0x04, "?  9", KEYCODE_9, PORT_CHAR('?') PORT_CHAR('9') PORT_CHAR('|'))
	MS1504_KEY(0x08, "%  0", KEYCODE_0, PORT_CHAR('%') PORT_CHAR('0') PORT_CHAR('['))
	MS1504_KEY(0x10, "!  =", KEYCODE_MINUS, PORT_CHAR('!') PORT_CHAR('=') PORT_CHAR(']'))
	MS1504_KEY(0x20, ";  $", KEYCODE_EQUALS, PORT_CHAR(';') PORT_CHAR('$') PORT_CHAR('`'))
	MS1504_KEY(0x40, "Backspace", KEYCODE_BACKSPACE, PORT_CHAR(8))
	MS1504_KEY(0x80, "Tab", KEYCODE_TAB, PORT_CHAR(9))

	PORT_START("ROW2")
	MS1504_KEY(0x01, "Q  Й", KEYCODE_Q, PORT_CHAR('q') PORT_CHAR('Q'))
	MS1504_KEY(0x02, "W  Ц", KEYCODE_W, PORT_CHAR('w') PORT_CHAR('W'))
	MS1504_KEY(0x04, "E  У", KEYCODE_E, PORT_CHAR('e') PORT_CHAR('E'))
	MS1504_KEY(0x08, "R  К", KEYCODE_R, PORT_CHAR('r') PORT_CHAR('R'))
	MS1504_KEY(0x10, "T  Е", KEYCODE_T, PORT_CHAR('t') PORT_CHAR('T'))
	MS1504_KEY(0x20, "Y  Н", KEYCODE_Y, PORT_CHAR('y') PORT_CHAR('Y'))
	MS1504_KEY(0x40, "U  Г", KEYCODE_U, PORT_CHAR('u') PORT_CHAR('U'))
	MS1504_KEY(0x80, "I  Ш", KEYCODE_I, PORT_CHAR('i') PORT_CHAR('I'))

	PORT_START("ROW3")
	MS1504_KEY(0x01, "O  Щ", KEYCODE_O, PORT_CHAR('o') PORT_CHAR('O'))
	MS1504_KEY(0x02, "P  З", KEYCODE_P, PORT_CHAR('p') PORT_CHAR('P'))
	MS1504_KEY(0x04, "Х", KEYCODE_OPENBRACE)
	MS1504_KEY(0x08, "Ъ", KEYCODE_CLOSEBRACE)
	MS1504_KEY(0x10, "Enter", KEYCODE_ENTER, PORT_CODE(KEYCODE_ENTER_PAD) PORT_CHAR(13))
	MS1504_KEY(0x20, "Ctrl", KEYCODE_LCONTROL, PORT_CHAR(UCHAR_MAMEKEY(LCONTROL)))
	MS1504_KEY(0x40, "A  Ф", KEYCODE_A, PORT_CHAR('a') PORT_CHAR('A'))
	MS1504_KEY(0x80, "S  Ы", KEYCODE_S, PORT_CHAR('s') PORT_CHAR('S'))

	PORT_START("ROW4")
	MS1504_KEY(0x01, "D  В", KEYCODE_D, PORT_CHAR('d') PORT_CHAR('D'))
	MS1504_KEY(0x02, "F  А", KEYCODE_F, PORT_CHAR('f') PORT_CHAR('F'))
	MS1504_KEY(0x04, "G  П", KEYCODE_G, PORT_CHAR('g') PORT_CHAR('G'))
	MS1504_KEY(0x08, "H  Р", KEYCODE_H, PORT_CHAR('h') PORT_CHAR('H'))
	MS1504_KEY(0x10, "J  О", KEYCODE_J, PORT_CHAR('j') PORT_CHAR('J'))
	MS1504_KEY(0x20, "K  Л", KEYCODE_K, PORT_CHAR('k') PORT_CHAR('K'))
	MS1504_KEY(0x40, "L  Д", KEYCODE_L, PORT_CHAR('l') PORT_CHAR('L'))
	MS1504_KEY(0x80, "Ж", KEYCODE_COLON)

	PORT_START("ROW5")
	MS1504_KEY(0x01, "Э", KEYCODE_QUOTE)
	MS1504_KEY(0x02, "*  #", KEYCODE_TILDE, PORT_CHAR('*') PORT_CHAR('#') PORT_CHAR('~'))
	MS1504_KEY(0x04, "Left Shift", KEYCODE_LSHIFT, PORT_CHAR(UCHAR_SHIFT_1))
	MS1504_KEY(0x08, "(  +", KEYCODE_BACKSLASH, PORT_CHAR('(') PORT_CHAR('+') PORT_CHAR('\''))
	MS1504_KEY(0x10, "Z  Я", KEYCODE_Z, PORT_CHAR('z') PORT_CHAR('Z'))
	MS1504_KEY(0x20, "X  Ч", KEYCODE_X, PORT_CHAR('x') PORT_CHAR('X'))
	MS1504_KEY(0x40, "C  С", KEYCODE_C, PORT_CHAR('c') PORT_CHAR('C'))
	MS1504_KEY(0x80, "V  М", KEYCODE_V, PORT_CHAR('v') PORT_CHAR('V'))

	PORT_START("ROW6")
	MS1504_KEY(0x01, "B  И", KEYCODE_B, PORT_CHAR('b') PORT_CHAR('B'))
	MS1504_KEY(0x02, "N  Т", KEYCODE_N, PORT_CHAR('n') PORT_CHAR('N'))
	MS1504_KEY(0x04, "M  Ь", KEYCODE_M, PORT_CHAR('m') PORT_CHAR('M'))
	MS1504_KEY(0x08, "Б", KEYCODE_COMMA)
	MS1504_KEY(0x10, "Ю", KEYCODE_STOP)
	MS1504_KEY(0x20, "Ё", KEYCODE_SLASH)
	MS1504_KEY(0x40, "Right Shift", KEYCODE_RSHIFT)
	MS1504_KEY(0x80, "PrtSc  *", KEYCODE_ASTERISK)

	PORT_START("ROW7")
	MS1504_KEY(0x01, "Alt", KEYCODE_LALT, PORT_CHAR(UCHAR_MAMEKEY(LALT)))
	MS1504_KEY(0x02, "Space", KEYCODE_SPACE, PORT_CHAR(' '))
	MS1504_KEY(0x04, "Caps Lock", KEYCODE_CAPSLOCK, PORT_CHAR(UCHAR_MAMEKEY(CAPSLOCK)))
	MS1504_KEY(0x08, "F1", KEYCODE_F1, PORT_CHAR(UCHAR_MAMEKEY(F1)))
	MS1504_KEY(0x10, "F2", KEYCODE_F2, PORT_CHAR(UCHAR_MAMEKEY(F2)))
	MS1504_KEY(0x20, "F3", KEYCODE_F3, PORT_CHAR(UCHAR_MAMEKEY(F3)))
	MS1504_KEY(0x40, "F4", KEYCODE_F4, PORT_CHAR(UCHAR_MAMEKEY(F4)))
	MS1504_KEY(0x80, "F5", KEYCODE_F5, PORT_CHAR(UCHAR_MAMEKEY(F5)))

	PORT_START("ROW8")
	MS1504_KEY(0x01, "F6", KEYCODE_F6, PORT_CHAR(UCHAR_MAMEKEY(F6)))
	MS1504_KEY(0x02, "F7", KEYCODE_F7, PORT_CHAR(UCHAR_MAMEKEY(F7)))
	MS1504_KEY(0x04, "F8", KEYCODE_F8, PORT_CHAR(UCHAR_MAMEKEY(F8)))
	MS1504_KEY(0x08, "F9", KEYCODE_F9, PORT_CHAR(UCHAR_MAMEKEY(F9)))
	MS1504_KEY(0x10, "F10", KEYCODE_F10, PORT_CHAR(UCHAR_MAMEKEY(F10)))
	MS1504_KEY(0x20, "Num Lock", KEYCODE_NUMLOCK)
	MS1504_KEY(0x40, "Scroll Lock", KEYCODE_SCRLOCK)
	MS1504_KEY(0x80, "7  Home", KEYCODE_7_PAD, PORT_CODE(KEYCODE_HOME) PORT_CHAR(UCHAR_MAMEKEY(HOME)))

	PORT_START("ROW9")
	MS1504_KEY(0x01, "8  Up", KEYCODE_8_PAD, PORT_CODE(KEYCODE_UP) PORT_CHAR(UCHAR_MAMEKEY(UP)))
	MS1504_KEY(0x02, "9  PgUp", KEYCODE_9_PAD, PORT_CODE(KEYCODE_PGUP) PORT_CHAR(UCHAR_MAMEKEY(PGUP)))
	MS1504_KEY(0x04, "Keypad -", KEYCODE_MINUS_PAD)
	MS1504_KEY(0x08, "4  Left", KEYCODE_4_PAD, PORT_CODE(KEYCODE_LEFT) PORT_CHAR(UCHAR_MAMEKEY(LEFT)))
	MS1504_KEY(0x10, "Keypad 5", KEYCODE_5_PAD)
	MS1504_KEY(0x20, "6  Right", KEYCODE_6_PAD, PORT_CODE(KEYCODE_RIGHT) PORT_CHAR(UCHAR_MAMEKEY(RIGHT)))
	MS1504_KEY(0x40, "Keypad +", KEYCODE_PLUS_PAD)
	MS1504_KEY(0x80, "1  End", KEYCODE_1_PAD, PORT_CODE(KEYCODE_END) PORT_CHAR(UCHAR_MAMEKEY(END)))

	PORT_START("ROW10")
	MS1504_KEY(0x01, "2  Down", KEYCODE_2_PAD, PORT_CODE(KEYCODE_DOWN) PORT_CHAR(UCHAR_MAMEKEY(DOWN)))
	MS1504_KEY(0x02, "3  PgDn", KEYCODE_3_PAD, PORT_CODE(KEYCODE_PGDN) PORT_CHAR(UCHAR_MAMEKEY(PGDN)))
	MS1504_KEY(0x04, "0  Ins", KEYCODE_0_PAD, PORT_CODE(KEYCODE_INSERT) PORT_CHAR(UCHAR_MAMEKEY(INSERT)))
	MS1504_KEY(0x08, ".  Del", KEYCODE_DEL_PAD, PORT_CODE(KEYCODE_DEL) PORT_CHAR(UCHAR_MAMEKEY(DEL)))
	MS1504_KEY(0x10, "Sys Req", KEYCODE_PRTSCR)
	PORT_BIT(0x60, IP_ACTIVE_HIGH, IPT_UNUSED)
	MS1504_KEY(0x80, "F11", KEYCODE_F11, PORT_CHAR(UCHAR_MAMEKEY(F11)))

	PORT_START("ROW11")
	MS1504_KEY(0x01, "F12", KEYCODE_F12, PORT_CHAR(UCHAR_MAMEKEY(F12)))
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_UNUSED)
	MS1504_KEY(0x04, "Rus/Lat (held)", KEYCODE_BACKSLASH2, PORT_CODE(KEYCODE_RALT) PORT_CHAR(UCHAR_SHIFT_2))
	MS1504_KEY(0x08, "Rus/Lat", KEYCODE_RCONTROL)
	PORT_BIT(0xf0, IP_ACTIVE_HIGH, IPT_UNUSED)
INPUT_PORTS_END

} // anonymous namespace

DEFINE_DEVICE_TYPE_PRIVATE(MS1504_KEYBOARD, device_t, ms1504_keyboard_device, "ms1504_kbd", "MS 1504 keyboard")

namespace {

ms1504_keyboard_device::ms1504_keyboard_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, MS1504_KEYBOARD, tag, owner, clock)
	, device_pc_kbd_interface(mconfig, *this)
	, m_rows(*this, "ROW%u", 0U)
	, m_scan_timer(nullptr)
	, m_bit_timer(nullptr)
{
}

ioport_constructor ms1504_keyboard_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(ms1504_keyboard);
}

void ms1504_keyboard_device::device_start()
{
	set_pc_kbdc_device();
	m_scan_timer = timer_alloc(FUNC(ms1504_keyboard_device::scan_tick), this);
	m_bit_timer = timer_alloc(FUNC(ms1504_keyboard_device::bit_tick), this);

	save_item(NAME(m_state));
	save_item(NAME(m_fifo));
	save_item(NAME(m_fifo_head));
	save_item(NAME(m_fifo_count));
	save_item(NAME(m_shift));
	save_item(NAME(m_bits));
	save_item(NAME(m_phase));
	save_item(NAME(m_kbclk));
	save_item(NAME(m_host_low));
	save_item(NAME(m_host_low_since));
	save_item(NAME(m_repeat_code));
	save_item(NAME(m_repeat_ticks));
}

void ms1504_keyboard_device::device_reset()
{
	std::fill(std::begin(m_state), std::end(m_state), 0);
	m_fifo_head = m_fifo_count = 0;
	m_shift = 0;
	m_bits = 0;
	m_phase = 0;
	m_kbclk = 1;
	m_host_low = false;
	m_host_low_since = attotime::zero;
	m_repeat_code = 0;
	m_repeat_ticks = 0;
	m_pc_kbdc->clock_write_from_kb(1);
	m_pc_kbdc->data_write_from_kb(1);
	m_bit_timer->adjust(attotime::never);
	m_scan_timer->adjust(attotime::from_msec(5), 0, attotime::from_msec(5));
}

void ms1504_keyboard_device::queue(uint8_t code)
{
	LOGMASKED(LOG_KBD, "scan %02x\n", code);
	if (m_fifo_count < int(std::size(m_fifo)))
	{
		m_fifo[(m_fifo_head + m_fifo_count) % std::size(m_fifo)] = code;
		m_fifo_count++;
	}
	try_send();
}

void ms1504_keyboard_device::try_send()
{
	if (m_bits || !m_fifo_count || m_host_low || !clock_signal() || !data_signal())
		return;
	m_shift = (m_fifo[m_fifo_head] << 1) | 1;
	m_fifo_head = (m_fifo_head + 1) % std::size(m_fifo);
	m_fifo_count--;
	m_bits = 9;
	m_phase = 0;
	m_bit_timer->adjust(attotime::from_usec(20));
}

TIMER_CALLBACK_MEMBER(ms1504_keyboard_device::bit_tick)
{
	switch (m_phase)
	{
	case 0:
		m_pc_kbdc->data_write_from_kb(BIT(m_shift, 0));
		m_phase = 1;
		m_bit_timer->adjust(attotime::from_usec(20));
		break;
	case 1:
		m_kbclk = 0;
		m_pc_kbdc->clock_write_from_kb(0);
		m_phase = 2;
		m_bit_timer->adjust(attotime::from_usec(40));
		break;
	case 2:
		m_kbclk = 1;
		m_pc_kbdc->clock_write_from_kb(1);
		m_shift >>= 1;
		if (--m_bits)
		{
			m_phase = 0;
			m_bit_timer->adjust(attotime::from_usec(40));
		}
		else
		{
			m_pc_kbdc->data_write_from_kb(1);
		}
		break;
	}
}

TIMER_CALLBACK_MEMBER(ms1504_keyboard_device::scan_tick)
{
	for (int r = 0; r < ROWS; r++)
	{
		uint8_t const now = m_rows[r]->read();
		uint8_t const changed = now ^ m_state[r];
		for (int b = 0; changed && b < 8; b++)
		{
			if (!BIT(changed, b))
				continue;
			uint8_t const code = r * 8 + b;
			if (BIT(now, b))
			{
				queue(code);
				m_repeat_code = code;
				m_repeat_ticks = 100;
			}
			else
			{
				queue(code | 0x80);
				if (m_repeat_code == code)
					m_repeat_code = 0;
			}
		}
		m_state[r] = now;
	}

	if (m_repeat_code && !--m_repeat_ticks)
	{
		queue(m_repeat_code);
		m_repeat_ticks = 20;
	}
	try_send();
}

void ms1504_keyboard_device::clock_write(int state)
{
	if (!m_kbclk)
		return;
	if (!state)
	{
		if (!m_host_low)
		{
			m_host_low = true;
			m_host_low_since = machine().time();
		}
	}
	else if (m_host_low)
	{
		m_host_low = false;
		if (machine().time() - m_host_low_since > attotime::from_msec(10))
		{
			m_fifo_count = 0;
			m_repeat_code = 0;
			queue(0xaa);
		}
	}
}

void ms1504_keyboard_device::data_write(int state)
{
	if (state)
		try_send();
}

class ms1504_state : public driver_device
{
public:
	ms1504_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_rtc(*this, "rtc")
	{ }

	void ms1504(machine_config &config) ATTR_COLD;

private:
	required_device<cpu_device> m_maincpu;
	required_device<mc146818_device> m_rtc;

	void speed_w(uint8_t data);

	virtual void machine_reset() override ATTR_COLD;

	static void cfg_fdc(device_t *device);
	static void internal_cards(device_slot_interface &device);
	static void keyboards(device_slot_interface &device);

	void mem_map(address_map &map) ATTR_COLD;
	void io_map(address_map &map) ATTR_COLD;
};

// The BIOS keyboard handler writes 00h on Ctrl+Alt+PgUp and 80h on Ctrl+Alt+PgDn
void ms1504_state::speed_w(uint8_t data)
{
	LOGMASKED(LOG_PORTS, "%s: speed <- %02x\n", machine().describe_context(), data);
	m_maincpu->set_unscaled_clock(XTAL(14'318'181) / (BIT(data, 7) ? 3 : 2));
}

void ms1504_state::machine_reset()
{
	m_maincpu->set_unscaled_clock(XTAL(14'318'181) / 2);
}

void ms1504_state::mem_map(address_map &map)
{
	map.unmap_value_high();
	map(0xf8000, 0xfffff).rom().region("bios", 0x8000);
}

void ms1504_state::io_map(address_map &map)
{
	map.unmap_value_high();
	map(0x0000, 0x00ff).m("mb", FUNC(ibm5160_mb_device::map));
	map(0x0070, 0x0070).w(m_rtc, FUNC(mc146818_device::address_w));
	map(0x0071, 0x0071).rw(m_rtc, FUNC(mc146818_device::data_r), FUNC(mc146818_device::data_w));
	map(0x00e0, 0x00e0).w(FUNC(ms1504_state::speed_w));
}

void ms1504_state::cfg_fdc(device_t *device)
{
	for (const char *tag : { "fdc:0", "fdc:1" })
	{
		auto &slot = dynamic_cast<device_slot_interface &>(*device->subdevice(tag));
		slot.set_default_option("35dd");
		slot.set_fixed(true);
	}
}

void ms1504_state::internal_cards(device_slot_interface &device)
{
	device.option_add_internal("lcdc", MS1504_LCDC);
	device.option_add_internal("fdc", ISA8_FDC_AT);
	device.option_add_internal("lpt", ISA8_LPT);
	device.option_add_internal("com", ISA8_COM);
}

void ms1504_state::keyboards(device_slot_interface &device)
{
	device.option_add("ms1504", MS1504_KEYBOARD);
}

static DEVICE_INPUT_DEFAULTS_START( ms1504_mb )
	DEVICE_INPUT_DEFAULTS("DSW0", 0x30, 0x20)
DEVICE_INPUT_DEFAULTS_END

void ms1504_state::ms1504(machine_config &config)
{
	I8086(config, m_maincpu, XTAL(14'318'181) / 3);
	m_maincpu->set_addrmap(AS_PROGRAM, &ms1504_state::mem_map);
	m_maincpu->set_addrmap(AS_IO, &ms1504_state::io_map);
	m_maincpu->set_irq_acknowledge_callback("mb:pic8259", FUNC(pic8259_device::inta_cb));

	ibm5160_mb_device &mb(IBM5160_MOTHERBOARD(config, "mb"));
	mb.set_cputag(m_maincpu);
	mb.int_callback().set_inputline(m_maincpu, 0);
	mb.nmi_callback().set_inputline(m_maincpu, INPUT_LINE_NMI);
	mb.kbdclk_callback().set("kbd", FUNC(pc_kbdc_device::clock_write_from_mb));
	mb.kbddata_callback().set("kbd", FUNC(pc_kbdc_device::data_write_from_mb));
	mb.set_input_default(DEVICE_INPUT_DEFAULTS_NAME(ms1504_mb));

	MC146818(config, m_rtc, 32.768_kHz_XTAL);

	ISA8_SLOT(config, "lcdc", 0, "mb:isa", internal_cards, "lcdc", true);
	ISA8_SLOT(config, "fdc", 0, "mb:isa", internal_cards, "fdc", true).set_option_machine_config("fdc", cfg_fdc);
	ISA8_SLOT(config, "lpt", 0, "mb:isa", internal_cards, "lpt", true);
	ISA8_SLOT(config, "com", 0, "mb:isa", internal_cards, "com", true);

	pc_kbdc_device &kbd(PC_KBDC(config, "kbd", keyboards, "ms1504"));
	kbd.out_clock_cb().set("mb", FUNC(ibm5160_mb_device::keyboard_clock_w));
	kbd.out_data_cb().set("mb", FUNC(ibm5160_mb_device::keyboard_data_w));

	RAM(config, RAM_TAG).set_default_size("640K");
}

ROM_START( ms1504 )
	ROM_REGION16_LE(0x10000, "bios", 0)
	ROM_SYSTEM_BIOS(0, "v280690", "28/06/90")
	ROMX_LOAD("pk300_mc1504_27c512.bin", 0x00000, 0x10000, CRC(f0234773) SHA1(a8783fa55f584750ac0d96fcfb78adb2b7a717f2), ROM_BIOS(0))
	ROM_SYSTEM_BIOS(1, "v180293", "18/02/93, XTA hard disk")
	ROMX_LOAD("ms1504_hdd_bios.256", 0x08000, 0x08000, CRC(e4418f13) SHA1(008ee600cab7222b6dea2f89273af34a955bdd88), ROM_BIOS(1))

	// battery-backed RTC as the BIOS leaves it: register A = 26h, register B = 02h
	ROM_REGION(0x40, "rtc", ROMREGION_ERASE00)
	ROM_FILL(0x0a, 1, 0x26)
	ROM_FILL(0x0b, 1, 0x02)
ROM_END

} // anonymous namespace

//    YEAR  NAME    PARENT  COMPAT  MACHINE  INPUT  CLASS         INIT        COMPANY        FULLNAME                    FLAGS
COMP( 1991, ms1504, 0,      0,      ms1504,  0,     ms1504_state, empty_init, "Elektronika", "Elektronika MS 1504 (PK-300)", 0 )
