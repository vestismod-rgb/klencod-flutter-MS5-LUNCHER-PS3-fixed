#ifndef SCENE_H
#define SCENE_H
#include <stdint.h>
#include "gfx.h"
/* Generated artwork derived from a hash: full-screen background and small tile art. */
void scene_render(Surf *dst, uint32_t hash, int with_vehicle);           /* static layers (sky, hills/city/waves, shade) */
void scene_overlay(Surf *dst, uint32_t hash, int with_vehicle, float frame); /* animated clouds + vehicle */
void scene_tile(Surf *dst, int x, int y, int size, int radius, uint32_t hash);
uint32_t scene_accent(uint32_t hash);   /* current accent colour (hash ignored) */
#define SCENE_NBG 6
#define SCENE_NACC 6
void scene_set_theme(int bg, int accent, int waves);
uint32_t scene_swatch_bg(int i);
uint32_t scene_swatch_accent(int i);
#endif
