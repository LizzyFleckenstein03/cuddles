#include "font.h"
#include "font_classic.h"
#include "gfx.h"
#include "heap.h"
#include "memory.h"
#include "math.h"
#include "bootinfo.h"

#include "font_builtin.c"

// important: must be a multiple of 2, else code won't work
#define TAB_SIZE 4

#define CHAR_WIDTH  8
#define CHAR_HEIGHT 16

static u8 *font; // u8[256][16]

static u16 font_size;

static u16 outer_width, outer_height;
static u16 cursor_x, cursor_y;

static u16 screen_width, screen_height;

void font_init()
{
	font = kmalloc(256 * 16);
}

void font_set_size(u16 size)
{
	font_size = size;

	outer_width  = CHAR_WIDTH  * font_size;
	outer_height = CHAR_HEIGHT * font_size;

	screen_width  = bootinfo->gfx_width  / outer_width;
	screen_height = bootinfo->gfx_height / outer_height;
}

void font_set_cursor(term_pos new_cursor)
{
	cursor_x = new_cursor.x;
	cursor_y = new_cursor.y;
}

term_pos font_get_cursor()
{
	return (term_pos){cursor_x, cursor_y};
}

term_pos font_get_size()
{
	return (term_pos){screen_width, screen_height};
}

void font_load_blob(const void *blob)
{
	lmemcpy(font, blob, 256*16);
}

void font_load_builtin()
{
	lmemcpy(font, fs_fonts_ter_u16n_cuddlefont, 256*16);
}

void font_load_classic()
{
	memset(font, 0, 256 * 16);

	classic_char *cfont = font_classic();

	int scale = 2;
	int xpad = (CHAR_WIDTH  - CLASSIC_CHAR_WIDTH  * scale) / 2;
	int ypad = (CHAR_HEIGHT - CLASSIC_CHAR_HEIGHT * scale) / 2;

	for (int i = 0; i < 255; i++)
	for (int xc = 0; xc < CLASSIC_CHAR_WIDTH;  xc++)
	for (int yc = 0; yc < CLASSIC_CHAR_HEIGHT; yc++) {
		if (!cfont[i].data[yc * CLASSIC_CHAR_WIDTH + xc])
			continue;

		for (int xf = 0; xf < scale; xf++)
		for (int yf = 0; yf < scale; yf++) {
			int x = xc * scale + xpad + xf;
			int y = yc * scale + ypad + yf;

			font[i * CHAR_HEIGHT + y] |= (1 << x);
		}
	}

	kfree(cfont);
}

void font_clear_screen()
{
	cursor_x = cursor_y = 0;
	gfx_set_area(0, 0, bootinfo->gfx_width, bootinfo->gfx_height, 0xFF000000);
}

static void render_char(u8 c)
{
	u16 base_x = cursor_x * outer_width;
	u16 base_y = cursor_y * outer_height;

	gfx_set_area(base_x, base_y, outer_width, outer_height, 0xFF000000);

	for (u16 x = 0; x < CHAR_WIDTH;  x++)
	for (u16 y = 0; y < CHAR_HEIGHT; y++) {
		if (!(font[c * CHAR_HEIGHT + y] & (1 << x)))
			continue;

		gfx_set_area(
			base_x + x * font_size,
			base_y + y * font_size,
			font_size, font_size, 0xFFFFFFFF);
	}
}

static void update_cursor()
{
	while (cursor_x >= screen_width) {
		cursor_x -= screen_width;
		cursor_y++;
	}

	while (cursor_y >= screen_height) {
		cursor_y--;

		lmemcpy(bootinfo->gfx_framebuffer,
			bootinfo->gfx_framebuffer + bootinfo->gfx_pitch * outer_height,
			bootinfo->gfx_pitch * (bootinfo->gfx_height - outer_height));

		gfx_set_area(0, bootinfo->gfx_height-outer_height, bootinfo->gfx_width, outer_height, 0xFF000000);
	}

	gfx_set_area(cursor_x * outer_width, cursor_y * outer_height,
		outer_width, outer_height, 0xFFFFFFFF);
}

void print_char(char c)
{
	switch (c) {
		case '\n':
			render_char(' ');
			cursor_y++;
			cursor_x = 0;
			break;

		case '\t':
			render_char(' ');
			cursor_x = (cursor_x + TAB_SIZE) & ~(TAB_SIZE - 1);
			break;

		case '\b':
			if (cursor_x > 0) {
				render_char(' ');
				cursor_x--;
			}
			break;

		case '\r':
			render_char(' ');
			cursor_x = 0;
			break;

		case '\v':
			// vertical tab intentionally unimplemented
			break;

		case '\a':
			// todo: bell
			break;

		case '\f':
			font_clear_screen();
			break;

		default:
			render_char(c);
			cursor_x++;
	}

	update_cursor();
}

void print(str line)
{
	for (usize i = 0; i < line.len; i++)
		print_char(line.data[i]);
}

void print_num_pad(u64 x, u8 base, u8 pad_len, char pad_char)
{
	char buffer[64];
	usize idx = 64;

	do {
		u8 digit = x % base;
		buffer[--idx] = digit + (digit < 10 ? '0' : ('A' - 10));
		x /= base;
	} while (x != 0);

	while (idx > (usize) (64 - pad_len))
		buffer[--idx] = pad_char;

	print((str) { 64 - idx, &buffer[idx] });
}

void print_num(u64 x, u8 base)
{
	print_num_pad(x, base, 0, ' ');
}

void print_dec(u64 x)
{
	print_num(x, 10);
}

void print_hex(u64 x)
{
	print_num(x, 16);
}

void print_dbl(double d, u8 points)
{
	if (d < 0) {
		print_char('-');
		d = -d;
	}

	long i = d;
	print_dec(i);
	print_char('.');
	print_num_pad((d - (double) i) * (double) ipow(10, points), 10, points, '0');
}

void print_bytes(usize bytes)
{
	static char fmt[] = { ' ', 'K', 'M', 'G', 'T' };
	usize unit = ipow(1000, LEN(fmt)-1);
	for (usize i = 0; i < LEN(fmt); i++) {
		if (bytes >= unit || unit == 1) {
			print_num_pad(bytes/unit, 10, 3, ' ');
			print_char('.');
			print_dec((bytes%unit)*10/unit);
			print_char(' ');
			print_char(fmt[LEN(fmt)-1-i]);
			print_char('B');
			break;
		}
		unit /= 1000;
	}
}
