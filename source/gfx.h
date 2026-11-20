#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include "textdata.h"

#define SW 1280
#define SH 720

typedef struct { uint32_t *px; int w, h; } Surf;

static inline uint32_t rgb(int r, int g, int b) { return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b; }
uint32_t mix(uint32_t a, uint32_t b, int t); /* t 0..255: a -> b */

void gfx_fill(Surf *s, uint32_t c);
void gfx_copy(Surf *dst, const Surf *src);
void gfx_crossfade(Surf *dst, const Surf *a, const Surf *b, int t);
void gfx_rect(Surf *s, int x, int y, int w, int h, uint32_t c, int a);
void gfx_rrect(Surf *s, int x, int y, int w, int h, int r, uint32_t c, int a);
void gfx_rrect_outline(Surf *s, int x, int y, int w, int h, int r, int t, uint32_t c, int a);
void gfx_circle(Surf *s, int cx, int cy, int r, uint32_t c, int a);
void gfx_ring(Surf *s, int cx, int cy, int r, int thick, uint32_t c, int a);
void gfx_tri(Surf *s, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c, int a);
void gfx_line(Surf *s, int x0, int y0, int x1, int y1, int thick, uint32_t c, int a);
void gfx_vgrad(Surf *s, int x, int y, int w, int h, uint32_t top, uint32_t bottom, int a);
void gfx_label(Surf *s, const Label *l, int x, int y, uint32_t c, int a);
int  gfx_text(Surf *s, const Font *f, int x, int y, const char *str, uint32_t c, int a);
int  gfx_text_w(const Font *f, const char *str);
#endif
