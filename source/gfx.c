#include <math.h>
#include <string.h>
#include "gfx.h"

static inline uint32_t blend(uint32_t d, uint32_t s, int a)
{
    if (a >= 255) return s;
    if (a <= 0) return d;
    int ia = 255 - a;
    uint32_t r = (((s >> 16) & 255) * a + ((d >> 16) & 255) * ia) / 255;
    uint32_t g = (((s >> 8) & 255) * a + ((d >> 8) & 255) * ia) / 255;
    uint32_t b = ((s & 255) * a + (d & 255) * ia) / 255;
    return (r << 16) | (g << 8) | b;
}

uint32_t mix(uint32_t a, uint32_t b, int t) { return blend(a, b, t); }

static inline void put(Surf *s, int x, int y, uint32_t c, int a)
{
    if ((unsigned)x >= (unsigned)s->w || (unsigned)y >= (unsigned)s->h) return;
    uint32_t *p = &s->px[y * s->w + x];
    *p = blend(*p, c, a);
}

void gfx_fill(Surf *s, uint32_t c)
{
    int n = s->w * s->h;
    for (int i = 0; i < n; i++) s->px[i] = c;
}

void gfx_copy(Surf *dst, const Surf *src) { memcpy(dst->px, src->px, (size_t)src->w * src->h * 4); }

void gfx_crossfade(Surf *dst, const Surf *a, const Surf *b, int t)
{
    int n = dst->w * dst->h, it = 256 - t;
    for (int i = 0; i < n; i++) {
        uint32_t x = a->px[i], y = b->px[i];
        uint32_t r = (((x >> 16) & 255) * it + ((y >> 16) & 255) * t) >> 8;
        uint32_t g = (((x >> 8) & 255) * it + ((y >> 8) & 255) * t) >> 8;
        uint32_t bl = ((x & 255) * it + (y & 255) * t) >> 8;
        dst->px[i] = (r << 16) | (g << 8) | bl;
    }
}

void gfx_rect(Surf *s, int x, int y, int w, int h, uint32_t c, int a)
{
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > s->w) x1 = s->w;
    if (y1 > s->h) y1 = s->h;
    for (int j = y; j < y1; j++) {
        uint32_t *p = &s->px[j * s->w];
        if (a >= 255) for (int i = x; i < x1; i++) p[i] = c;
        else for (int i = x; i < x1; i++) p[i] = blend(p[i], c, a);
    }
}

/* coverage 0..255 of a rounded rect corner pixel */
static inline int corner_cov(int dx, int dy, int r)
{
    float d = sqrtf((float)(dx * dx + dy * dy));
    float c = (float)r - d + 0.5f;
    if (c <= 0) return 0;
    if (c >= 1) return 255;
    return (int)(c * 255);
}

void gfx_rrect(Surf *s, int x, int y, int w, int h, int r, uint32_t c, int a)
{
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r <= 0) { gfx_rect(s, x, y, w, h, c, a); return; }
    gfx_rect(s, x + r, y, w - 2 * r, h, c, a);
    gfx_rect(s, x, y + r, r, h - 2 * r, c, a);
    gfx_rect(s, x + w - r, y + r, r, h - 2 * r, c, a);
    for (int j = 0; j < r; j++)
        for (int i = 0; i < r; i++) {
            int cov = corner_cov(r - 1 - i + 0, r - 1 - j + 0, r) * a / 255;
            if (!cov) continue;
            put(s, x + i, y + j, c, cov);
            put(s, x + w - 1 - i, y + j, c, cov);
            put(s, x + i, y + h - 1 - j, c, cov);
            put(s, x + w - 1 - i, y + h - 1 - j, c, cov);
        }
}

void gfx_circle(Surf *s, int cx, int cy, int r, uint32_t c, int a)
{
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) {
            int d2 = i * i + j * j;
            if (d2 > (r + 1) * (r + 1)) continue;
            int cov = (d2 <= (r - 1) * (r - 1)) ? 255 : corner_cov(i, j, r);
            if (cov) put(s, cx + i, cy + j, c, cov * a / 255);
        }
}

void gfx_ring(Surf *s, int cx, int cy, int r, int thick, uint32_t c, int a)
{
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) {
            int d2 = i * i + j * j;
            if (d2 > (r + 1) * (r + 1)) continue;
            float d = sqrtf((float)d2);
            float outer = (float)r - d + 0.5f, inner = d - (float)(r - thick) + 0.5f;
            float cv = outer < inner ? outer : inner;
            if (cv <= 0) continue;
            if (cv > 1) cv = 1;
            put(s, cx + i, cy + j, c, (int)(cv * 255) * a / 255);
        }
}

static int edge(int ax, int ay, int bx, int by, int px, int py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); }

void gfx_tri(Surf *s, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c, int a)
{
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= s->w) maxx = s->w - 1;
    if (maxy >= s->h) maxy = s->h - 1;
    int area = edge(x0, y0, x1, y1, x2, y2);
    if (!area) return;
    for (int y = miny; y <= maxy; y++)
        for (int x = minx; x <= maxx; x++) {
            int w0 = edge(x1, y1, x2, y2, x, y), w1 = edge(x2, y2, x0, y0, x, y), w2 = edge(x0, y0, x1, y1, x, y);
            if (area > 0 ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0))
                put(s, x, y, c, a);
        }
}

void gfx_line(Surf *s, int x0, int y0, int x1, int y1, int thick, uint32_t c, int a)
{
    float dx = (float)(x1 - x0), dy = (float)(y1 - y0), len = sqrtf(dx * dx + dy * dy);
    if (len < 0.5f) return;
    float nx = -dy / len * thick * 0.5f, ny = dx / len * thick * 0.5f;
    int ax = (int)(x0 + nx), ay = (int)(y0 + ny), bx = (int)(x0 - nx), by = (int)(y0 - ny);
    int cx = (int)(x1 - nx), cy = (int)(y1 - ny), ex = (int)(x1 + nx), ey = (int)(y1 + ny);
    gfx_tri(s, ax, ay, bx, by, cx, cy, c, a);
    gfx_tri(s, ax, ay, cx, cy, ex, ey, c, a);
}

void gfx_vgrad(Surf *s, int x, int y, int w, int h, uint32_t top, uint32_t bottom, int a)
{
    for (int j = 0; j < h; j++) {
        uint32_t c = mix(top, bottom, h > 1 ? j * 255 / (h - 1) : 0);
        gfx_rect(s, x, y + j, w, 1, c, a);
    }
}

void gfx_label(Surf *s, const Label *l, int x, int y, uint32_t c, int a)
{
    for (int j = 0; j < l->h; j++) {
        int py = y + j;
        if ((unsigned)py >= (unsigned)s->h) continue;
        for (int i = 0; i < l->w; i++) {
            int m = l->px[j * l->w + i];
            if (!m) continue;
            put(s, x + i, py, c, m * a / 255);
        }
    }
}

int gfx_text_w(const Font *f, const char *str)
{
    int w = 0;
    for (; *str; str++) {
        unsigned char ch = (unsigned char)*str;
        if (ch < 32 || ch > 126) ch = '?';
        w += f->g[ch - 32].adv;
    }
    return w;
}

int gfx_text(Surf *s, const Font *f, int x, int y, const char *str, uint32_t c, int a)
{
    int pen = x, base = y + f->ascent;
    for (; *str; str++) {
        unsigned char ch = (unsigned char)*str;
        if (ch < 32 || ch > 126) ch = '?';
        const Glyph *g = &f->g[ch - 32];
        const unsigned char *m = f->px + g->off;
        for (int j = 0; j < g->h; j++) {
            int py = base + g->by + j;
            if ((unsigned)py >= (unsigned)s->h) continue;
            for (int i = 0; i < g->w; i++) {
                int v = m[j * g->w + i];
                if (v) put(s, pen + g->bx + i, py, c, v * a / 255);
            }
        }
        pen += g->adv;
    }
    return pen - x;
}

/* Rounded-rectangle outline of thickness t (works on translucent panels, no interior fill). */
void gfx_rrect_outline(Surf *s, int x, int y, int w, int h, int r, int t, uint32_t c, int a)
{
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    gfx_rect(s, x + r, y, w - 2 * r, t, c, a);
    gfx_rect(s, x + r, y + h - t, w - 2 * r, t, c, a);
    gfx_rect(s, x, y + r, t, h - 2 * r, c, a);
    gfx_rect(s, x + w - t, y + r, t, h - 2 * r, c, a);
    for (int j = 0; j < r; j++)
        for (int i = 0; i < r; i++) {
            float dx = (float)(r - i) - 0.5f, dy = (float)(r - j) - 0.5f;
            float d = sqrtf(dx * dx + dy * dy);
            float cov = d > r ? 0.f : (d < r - t ? 0.f : 1.f);
            if (d > r - 0.5f && d <= r + 0.5f) cov = (r + 0.5f - d);
            if (d >= r - t - 0.5f && d < r - t + 0.5f) cov = (d - (r - t - 0.5f));
            int cv = (int)(cov * a);
            if (cv <= 0) continue;
            put(s, x + i, y + j, c, cv);
            put(s, x + w - 1 - i, y + j, c, cv);
            put(s, x + i, y + h - 1 - j, c, cv);
            put(s, x + w - 1 - i, y + h - 1 - j, c, cv);
        }
}
