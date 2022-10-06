#ifndef _GFX_H_
#define _GFX_H_

#include "def.h"

extern struct __attribute__((packed)) GfxInfo {
	u16 pitch;
	u16 width;
	u16 height;
	u32 framebuffer;
} *gfxinfo;

typedef struct {
	u8 r, g, b, a;
} color;

u32 make_color(color col);
void set_pixel(u16 x, u16 y, u32 col);
void set_region(u16 x, u16 y, u16 w, u16 h, u32 col);

#endif
