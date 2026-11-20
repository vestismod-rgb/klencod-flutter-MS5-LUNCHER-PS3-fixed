#include <math.h>
#include "scene.h"

typedef struct {
    int kind;          /* 0 hills, 1 city, 2 waves */
    int night;
    uint32_t sky_top, sky_bot, sun, far_c, near_c, accent;
    float p1, p2, p3;  /* phases */
} Pal;

static uint32_t rnd(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }

static uint32_t hsv(float h, float s, float v)
{
    h = fmodf(h, 360.0f); if (h < 0) h += 360.0f;
    float c = v * s, x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1)), m = v - c, r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; } else if (h < 120) { r = x; g = c; } else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; } else if (h < 300) { r = x; b = c; } else { r = c; b = x; }
    return rgb((int)((r + m) * 255), (int)((g + m) * 255), (int)((b + m) * 255));
}

static Pal pal(uint32_t hash)
{
    Pal p;
    uint32_t s = hash ^ 0x9e3779b9u;
    float h = (float)(rnd(&s) % 360);
    p.kind = (int)(rnd(&s) % 3);
    p.night = (rnd(&s) % 100) < 40;
    if (p.night) {
        p.sky_top = hsv(h, .70f, .22f); p.sky_bot = hsv(h + 40, .60f, .62f);
        p.sun = hsv(h + 30, .12f, 1.0f); p.far_c = hsv(h + 20, .55f, .36f); p.near_c = hsv(h + 20, .65f, .16f);
    } else {
        p.sky_top = hsv(h, .55f, .62f); p.sky_bot = hsv(h + 28, .40f, 1.0f);
        p.sun = hsv(h + 35, .15f, 1.0f); p.far_c = hsv(h + 12, .55f, .62f); p.near_c = hsv(h + 12, .62f, .30f);
    }
    p.accent = hsv(h + 40, .65f, 1.0f);
    p.p1 = (float)(rnd(&s) % 628) / 100.0f; p.p2 = (float)(rnd(&s) % 628) / 100.0f; p.p3 = (float)(rnd(&s) % 628) / 100.0f;
    return p;
}



/* MS5 background: deep blue gradient, two soft glows, three translucent wave layers.
   The per-title hash only shifts the wave phase so the background still changes subtly. */
typedef struct { uint32_t b1, b2, b3, g1, g2, wv; } Theme;
static const Theme THEMES[SCENE_NBG] = {
    { 0x0d2a7a, 0x071647, 0x020617, 0x2f6bff, 0x6a5bff, 0x6aa0ff },  /* blue */
    { 0x2a1470, 0x150a45, 0x070318, 0x7a3bff, 0xd24bff, 0xa98aff },  /* violet */
    { 0x0a4a52, 0x062c36, 0x01141a, 0x18b7a0, 0x2f8bff, 0x5fe0c8 },  /* teal */
    { 0x6a1030, 0x3a0a22, 0x14030c, 0xff4a6a, 0xff8a3b, 0xff8aa0 },  /* crimson */
    { 0x7a3a0e, 0x40200a, 0x180a03, 0xff8a3b, 0xffc15a, 0xffb27a },  /* orange */
    { 0x2a3346, 0x161c2b, 0x07090f, 0x6a7a99, 0x4a5a8a, 0x9aa8c8 },  /* slate */
};
static const uint32_t ACCENTS[SCENE_NACC] = { 0x6aa3ff, 0xffffff, 0x4de0c4, 0xff6fa5, 0xffb347, 0xb48cff };
static int g_bg = 0, g_acc = 0, g_waves = 1;
#define MS5_B1 THEMES[g_bg].b1
#define MS5_B2 THEMES[g_bg].b2
#define MS5_B3 THEMES[g_bg].b3
#define MS5_G1 THEMES[g_bg].g1
#define MS5_G2 THEMES[g_bg].g2
#define MS5_WV THEMES[g_bg].wv
void scene_set_theme(int bg, int accent, int waves)
{
    g_waves = waves != 0;
    g_bg = (bg >= 0 && bg < SCENE_NBG) ? bg : 0;
    g_acc = (accent >= 0 && accent < SCENE_NACC) ? accent : 0;
}
uint32_t scene_swatch_bg(int i) { return THEMES[i].g1; }
uint32_t scene_swatch_accent(int i) { return ACCENTS[i]; }

static void wave_layer(Surf *dst, float base, float amp, float k, float ph, int alpha)
{
    for (int x = 0; x < SW; x++) {
        float u = x * k;
        int y0 = (int)(base + amp * (0.7f * sinf(u + ph) + 0.3f * sinf(u * 2.3f + ph * 1.6f)));
        if (y0 < 0) y0 = 0;
        for (int y = y0; y < SH; y += 1) {
            int a = alpha * (SH - y) / (SH - y0 + 1);
            if (a <= 0) break;
            uint32_t *p = &dst->px[y * dst->w + x];
            *p = mix(*p, MS5_WV, a);
        }
    }
}

void scene_render(Surf *dst, uint32_t hash, int with_vehicle)
{
    (void)with_vehicle;
    Pal p = pal(hash);
    gfx_vgrad(dst, 0, 0, SW, SH / 2, MS5_B1, MS5_B2, 255);
    gfx_vgrad(dst, 0, SH / 2, SW, SH - SH / 2, MS5_B2, MS5_B3, 255);
    for (int i = 6; i >= 1; i--) gfx_circle(dst, 920, 200, 60 + i * 55, MS5_G1, 10);
    for (int i = 6; i >= 1; i--) gfx_circle(dst, 190, 690, 50 + i * 50, MS5_G2, 8);
    if (g_waves) {
        wave_layer(dst, 400, 50, 0.0065f, p.p1, 70);
        wave_layer(dst, 490, 46, 0.0058f, p.p2 + 1.5f, 60);
        wave_layer(dst, 575, 40, 0.0072f, p.p3 + 3.0f, 50);
    }
    /* readability shade at top and bottom */
    for (int y = 0; y < 140; y++) gfx_rect(dst, 0, y, SW, 1, rgb(2, 6, 23), 110 - y * 110 / 140);
    for (int y = 540; y < SH; y++) gfx_rect(dst, 0, y, SW, 1, rgb(2, 6, 23), (y - 540) * 150 / (SH - 540));
}

void scene_overlay(Surf *dst, uint32_t hash, int with_vehicle, float frame)
{
    (void)with_vehicle; (void)hash;
    /* three light ribbons drifting slowly, plus floating motes */
    for (int r = 0; r < 3; r++) {
        float base = 330 + r * 95, amp = 42, k = 0.0062f + r * 0.0004f;
        float ph = frame * (0.018f + r * 0.006f) * (r == 1 ? -1.0f : 1.0f) + r * 2.1f;
        int px = 0, py = 0;
        for (int x = 0; x <= SW && g_waves; x += 16) {
            float u = x * k;
            int y = (int)(base + amp * (0.7f * sinf(u + ph) + 0.3f * sinf(u * 2.3f + ph * 1.6f)));
            int edge = x < SW / 2 ? x : SW - x;
            int a = edge * 150 / (SW / 2);
            if (x) gfx_line(dst, px, py, x, y, 2, rgb(207, 224, 255), a);
            px = x; py = y;
        }
    }
    static const int mx[8] = { 140, 330, 560, 820, 1010, 1190, 700, 90 };
    static const int my[8] = { 210, 520, 170, 590, 230, 470, 330, 610 };
    for (int i = 0; i < 8; i++) {
        int y = my[i] + (int)(10.0f * sinf(frame * 0.03f + i * 1.3f));
        int a = 90 + (int)(70.0f * sinf(frame * 0.05f + i));
        gfx_circle(dst, mx[i], y, 2 + (i & 1), rgb(255, 255, 255), a);
    }
}

void scene_tile(Surf *dst, int x, int y, int size, int radius, uint32_t hash)
{
    Pal p = pal(hash);
    /* draw into the surface directly, then re-round the corners by overpainting is not possible,
       so clip by drawing row spans that respect the rounded corner. */
    for (int j = 0; j < size; j++) {
        int inset = 0;
        if (j < radius) inset = radius - (int)sqrtf((float)(radius * radius - (radius - j) * (radius - j)));
        else if (j >= size - radius) { int k = size - 1 - j; inset = radius - (int)sqrtf((float)(radius * radius - (radius - k) * (radius - k))); }
        uint32_t row = mix(p.sky_top, p.sky_bot, j * 255 / size);
        for (int i = inset; i < size - inset; i++) {
            int px = x + i, py = y + j;
            if ((unsigned)px >= (unsigned)dst->w || (unsigned)py >= (unsigned)dst->h) continue;
            float u = (float)i / size;
            int hy1 = (int)(size * (0.62f + 0.07f * sinf(u * 6.0f + p.p1))), hy2 = (int)(size * (0.78f + 0.06f * sinf(u * 7.0f + p.p2 + 2)));
            float ddx = i - size * 0.72f, ddy = j - size * 0.30f;
            uint32_t c = row;
            if (ddx * ddx + ddy * ddy < size * size * 0.015f) c = p.sun;
            if (j > hy1) c = p.far_c;
            if (j > hy2) c = p.near_c;
            dst->px[py * dst->w + px] = c;
        }
    }
}

uint32_t scene_accent(uint32_t hash) { (void)hash; return ACCENTS[g_acc]; }
