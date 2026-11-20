#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pages.h"
#include "ui.h"
#include "scene.h"
#include "scan.h"

#define WHITE rgb(255, 255, 255)
#define MUTED rgb(205, 210, 240)
#define INK   rgb(10, 20, 60)
#define ACC   scene_accent(0)

/* ---------------- settings ---------------- */
void settings_theme_defaults(Settings *s)
{
    s->bg = 0; s->accent = 0; s->waves = 1; s->panel = 55; s->dim = 0; s->iconsz = 84; s->iconrad = 20;
}
void settings_defaults(Settings *s)
{
    memset(s, 0, sizeof *s);
    settings_theme_defaults(s);
    s->lang = 0; s->anim = 1; s->speed = 100; s->h24 = 1; s->askstart = 1; s->askplay = 1;
    s->users = 1; s->cur_user = 0;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int settings_load(Settings *s, const char *path)
{
    settings_defaults(s);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[64]; int v;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "lang=%d", &v) == 1) s->lang = clampi(v, 0, 1);
        else if (sscanf(line, "bg=%d", &v) == 1) s->bg = clampi(v, 0, SCENE_NBG - 1);
        else if (sscanf(line, "accent=%d", &v) == 1) s->accent = clampi(v, 0, SCENE_NACC - 1);
        else if (sscanf(line, "waves=%d", &v) == 1) s->waves = v != 0;
        else if (sscanf(line, "panel=%d", &v) == 1) s->panel = clampi(v, 0, 100);
        else if (sscanf(line, "dim=%d", &v) == 1) s->dim = clampi(v, 0, 80);
        else if (sscanf(line, "iconsz=%d", &v) == 1) s->iconsz = clampi(v, 64, 112);
        else if (sscanf(line, "iconrad=%d", &v) == 1) s->iconrad = clampi(v, 0, 40);
        else if (sscanf(line, "anim=%d", &v) == 1) s->anim = v != 0;
        else if (sscanf(line, "speed=%d", &v) == 1) s->speed = clampi(v, 50, 200);
        else if (sscanf(line, "h24=%d", &v) == 1) s->h24 = v != 0;
        else if (sscanf(line, "askstart=%d", &v) == 1) s->askstart = v != 0;
        else if (sscanf(line, "askplay=%d", &v) == 1) s->askplay = v != 0;
        else if (sscanf(line, "users=%d", &v) == 1) s->users = clampi(v, 1, MAXUSERS);
        else if (sscanf(line, "user=%d", &v) == 1) s->cur_user = clampi(v, 0, MAXUSERS - 1);
    }
    fclose(f);
    if (s->cur_user >= s->users) s->cur_user = s->users - 1;
    return 1;
}

int settings_save(const Settings *s, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "lang=%d\nbg=%d\naccent=%d\nwaves=%d\npanel=%d\ndim=%d\niconsz=%d\niconrad=%d\nanim=%d\nspeed=%d\nh24=%d\naskstart=%d\naskplay=%d\nusers=%d\nuser=%d\n",
            s->lang, s->bg, s->accent, s->waves, s->panel, s->dim, s->iconsz, s->iconrad, s->anim, s->speed, s->h24, s->askstart, s->askplay, s->users, s->cur_user);
    fclose(f);
    return 1;
}

/* ---------------- files ---------------- */
static void full(char *out, size_t n, const char *p) { snprintf(out, n, "%s%s", scan_root(), p); }

static int cmp_items(const void *a, const void *b)
{
    const FileItem *x = a, *y = b;
    if (x->dir != y->dir) return y->dir - x->dir;
    return strcasecmp(x->name, y->name);
}

void files_open(Files *f, const char *cwd)
{
    if (!f->items) f->items = (FileItem *)calloc(FILES_MAX, sizeof(FileItem));
    snprintf(f->cwd, sizeof f->cwd, "%s", cwd);
    f->n = 0; f->sel = 0; f->top = 0;
    if (!f->items) return;
    if (!cwd[0]) {
        static const char *roots[] = { "/dev_hdd0", "/dev_usb000", "/dev_usb001", "/dev_usb002", "/dev_usb003",
                                       "/dev_usb004", "/dev_usb005", "/dev_usb006", "/dev_usb007", "/dev_bdvd" };
        for (unsigned i = 0; i < sizeof roots / sizeof *roots; i++) {
            char p[300]; struct stat st;
            full(p, sizeof p, roots[i]);
            if (stat(p, &st) == 0 && S_ISDIR(st.st_mode)) {
                memset(&f->items[f->n], 0, sizeof f->items[0]);
                snprintf(f->items[f->n].name, sizeof f->items[0].name, "%s", roots[i] + 1);
                f->items[f->n].dir = 1; f->items[f->n].size = -1; f->n++;
            }
        }
        return;
    }
    char p[300];
    full(p, sizeof p, cwd);
    DIR *d = opendir(p);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && f->n < FILES_MAX) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char q[600]; struct stat st;
        snprintf(q, sizeof q, "%s/%s", p, e->d_name);
        if (stat(q, &st) != 0) continue;
        FileItem *it = &f->items[f->n++];
        memset(it, 0, sizeof *it);
        snprintf(it->name, sizeof it->name, "%s", e->d_name);
        it->dir = S_ISDIR(st.st_mode);
        it->size = it->dir ? -1 : (long long)st.st_size;
    }
    closedir(d);
    qsort(f->items, f->n, sizeof(FileItem), cmp_items);
}

void files_free(Files *f) { free(f->items); f->items = NULL; f->n = 0; }
const FileItem *files_cur(const Files *f) { return (f->n && f->sel < f->n) ? &f->items[f->sel] : NULL; }
int files_nsel(const Files *f) { int c = 0; for (int i = 0; i < f->n; i++) c += f->items[i].sel; return c; }
void files_select_all(Files *f, int on) { if (!f->cwd[0]) return; for (int i = 0; i < f->n; i++) f->items[i].sel = on; }

int files_enter(Files *f)
{
    const FileItem *it = files_cur(f);
    if (!it || !it->dir) return 0;
    char nc[256];
    snprintf(nc, sizeof nc, "%s/%s", f->cwd, it->name);
    files_open(f, nc);
    return 1;
}

int files_up(Files *f)
{
    if (!f->cwd[0]) return 0;
    char nc[256], last[96] = "";
    snprintf(nc, sizeof nc, "%s", f->cwd);
    char *sl = strrchr(nc, '/');
    if (sl) { snprintf(last, sizeof last, "%s", sl + 1); *sl = 0; }
    files_open(f, nc);
    for (int i = 0; i < f->n; i++) if (!strcmp(f->items[i].name, last)) { f->sel = i; break; }
    return 1;
}

int files_delete(Files *f)
{
    if (!f->cwd[0] || !f->n) return 1;           /* never delete device roots */
    int fails = 0, any = files_nsel(f) > 0;
    for (int i = 0; i < f->n; i++) {
        if (any ? !f->items[i].sel : i != f->sel) continue;
        char p[700];
        snprintf(p, sizeof p, "%s%s/%s", scan_root(), f->cwd, f->items[i].name);
        if (remove(p) != 0) fails++;              /* removes files and empty folders only */
    }
    int keep = f->sel;
    char cwd[256]; snprintf(cwd, sizeof cwd, "%s", f->cwd);
    files_open(f, cwd);
    f->sel = keep < f->n ? keep : (f->n ? f->n - 1 : 0);
    return fails;
}

/* ---------------- store ---------------- */
void store_refilter(Store *s)
{
    s->nvis = 0;
    for (int i = 0; i < s->n && s->nvis < 64; i++) {
        int game = !strcasecmp(s->items[i].kind, "game");
        if (s->filter == 0 || (s->filter == 1 && game) || (s->filter == 2 && !game)) s->vis[s->nvis++] = i;
    }
    s->sel = 0;
}

void store_load(Store *s, const char *path)
{
    store_free(s);
    FILE *f = fopen(path, "r");
    if (!f) return;
    s->items = (StoreItem *)calloc(64, sizeof(StoreItem));
    if (!s->items) { fclose(f); return; }
    char line[320];
    while (s->n < 64 && fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\r\n")] = 0;
        char *fld[4] = { line, NULL, NULL, NULL };
        int k = 0;
        for (char *c = line; *c && k < 3; c++) if (*c == '|') { *c = 0; fld[++k] = c + 1; }
        if (k < 3) continue;
        StoreItem *it = &s->items[s->n++];
        snprintf(it->name, sizeof it->name, "%s", fld[0]);
        snprintf(it->desc, sizeof it->desc, "%s", fld[1]);
        snprintf(it->kind, sizeof it->kind, "%s", fld[2]);
        snprintf(it->size, sizeof it->size, "%s", fld[3]);
        uint32_t h = 2166136261u;
        for (const char *c = it->name; *c; c++) { h ^= (unsigned char)*c; h *= 16777619u; }
        it->hash = h;
    }
    fclose(f);
    store_refilter(s);
}
void store_free(Store *s) { free(s->items); s->items = NULL; s->n = 0; s->sel = 0; s->nvis = 0; }

void page_clock(char *out, int n, int h24)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    int h = tm ? tm->tm_hour : 0, m = tm ? tm->tm_min : 0;
    if (h24) snprintf(out, n, "%02d:%02d", h, m);
    else snprintf(out, n, "%02d:%02d %s", h % 12 ? h % 12 : 12, m, h < 12 ? "AM" : "PM");
}

/* ---------------- settings pages ---------------- */
enum { RT_SWITCH, RT_SEG, RT_SLIDER, RT_SWATCH, RT_BTN, RT_NAME };
enum { F_LANG, F_SPEED, F_MOTION, F_24H, F_ASKSTART, F_ASKPLAY, F_BG, F_PANEL, F_DIM, F_WAVES, F_ACCENT, F_ICON, F_RAD,
       B_SWITCH, B_ADD, B_DEL, B_RESET, F_NONE };
typedef struct { int label, sub, type, id; } Row;

static const Row ROWS_USER[] = {
    { L_R_SWITCH, -1, RT_NAME, F_NONE },
    { L_R_SWITCH, -1, RT_BTN, B_SWITCH }, { L_R_ADD, -1, RT_BTN, B_ADD }, { L_R_DEL, -1, RT_BTN, B_DEL },
};
static const Row ROWS_GEN[] = {
    { L_R_LANG, -1, RT_SEG, F_LANG }, { L_R_SPEED, -1, RT_SLIDER, F_SPEED }, { L_R_MOTION, L_R_MOTION_D, RT_SWITCH, F_MOTION },
    { L_R_24H, -1, RT_SWITCH, F_24H }, { L_R_ASKSTART, L_R_ASKSTART_D, RT_SWITCH, F_ASKSTART }, { L_R_ASKPLAY, -1, RT_SWITCH, F_ASKPLAY },
};
static const Row ROWS_THEME[] = {
    { L_R_BG, -1, RT_SWATCH, F_BG }, { L_R_PANEL, -1, RT_SLIDER, F_PANEL }, { L_R_DIM, -1, RT_SLIDER, F_DIM },
    { L_R_WAVES, -1, RT_SWITCH, F_WAVES }, { L_R_ACCENT, -1, RT_SWATCH, F_ACCENT }, { L_R_ICON, -1, RT_SLIDER, F_ICON },
    { L_R_RAD, -1, RT_SLIDER, F_RAD }, { L_R_RESET, -1, RT_BTN, B_RESET },
};

static const Row *page_rows(int page, int *n)
{
    if (page == 1) { *n = (int)(sizeof ROWS_USER / sizeof *ROWS_USER); return ROWS_USER; }
    if (page == 2) { *n = (int)(sizeof ROWS_GEN / sizeof *ROWS_GEN); return ROWS_GEN; }
    *n = (int)(sizeof ROWS_THEME / sizeof *ROWS_THEME); return ROWS_THEME;
}

static int *fld(Settings *s, int id)
{
    switch (id) {
    case F_LANG: return &s->lang; case F_SPEED: return &s->speed; case F_MOTION: return &s->anim; case F_24H: return &s->h24;
    case F_ASKSTART: return &s->askstart; case F_ASKPLAY: return &s->askplay; case F_BG: return &s->bg; case F_PANEL: return &s->panel;
    case F_DIM: return &s->dim; case F_WAVES: return &s->waves; case F_ACCENT: return &s->accent; case F_ICON: return &s->iconsz;
    case F_RAD: return &s->iconrad;
    }
    return NULL;
}
static void slider_range(int id, int *lo, int *hi, int *step)
{
    switch (id) {
    case F_SPEED: *lo = 50; *hi = 200; *step = 25; break;
    case F_PANEL: *lo = 0; *hi = 100; *step = 5; break;
    case F_DIM: *lo = 0; *hi = 80; *step = 5; break;
    case F_ICON: *lo = 64; *hi = 112; *step = 4; break;
    default: *lo = 0; *hi = 40; *step = 2; break;
    }
}

void page_settings_input(Ui *u, int l, int r, int up, int dn, int ok, int back)
{
    Settings *s = &u->set;
    if (u->spage == 0) {
        if (l && u->scat > 0) u->scat--;
        if (r && u->scat < 2) u->scat++;
        if (ok) { u->spage = 1 + u->scat; u->srow = 0; }
        return;
    }
    if (back) { u->spage = 0; return; }
    int n; const Row *rows = page_rows(u->spage, &n);
    if (up && u->srow > 0) u->srow--;
    if (dn && u->srow < n - 1) u->srow++;
    if (u->spage == 1 && u->srow == 0 && dn) u->srow = 1;   /* name row is display only */
    const Row *row = &rows[u->srow];
    int d = r - l;
    int *v = fld(s, row->id);
    switch (row->type) {
    case RT_SWITCH: if (v && (ok || d)) *v = !*v; break;
    case RT_SEG: if (v && (ok || d)) *v = !*v; break;
    case RT_SWATCH: if (v && d) { int nn = row->id == F_BG ? SCENE_NBG : SCENE_NACC; *v = (*v + nn + d) % nn; } break;
    case RT_SLIDER: if (v && d) { int lo, hi, st; slider_range(row->id, &lo, &hi, &st); *v = clampi(*v + d * st, lo, hi); } break;
    case RT_BTN:
        if (!ok) break;
        if (row->id == B_SWITCH) s->cur_user = (s->cur_user + 1) % s->users;
        else if (row->id == B_ADD) { if (s->users < MAXUSERS) { s->users++; s->cur_user = s->users - 1; } }
        else if (row->id == B_DEL) { if (s->users > 1) { s->users--; if (s->cur_user >= s->users) s->cur_user = s->users - 1; } }
        else if (row->id == B_RESET) settings_theme_defaults(s);
        break;
    default: break;
    }
}

static void title(Ui *u, Surf *fb, int id) { tr_draw(fb, u, id, 64, 122, WHITE, 255); }

static void draw_switch(Surf *fb, int x, int cy, int on)
{
    gfx_rrect(fb, x, cy - 17, 54, 34, 17, on ? ACC : WHITE, on ? 255 : 70);
    gfx_circle(fb, on ? x + 37 : x + 17, cy, 13, WHITE, 255);
}

static void draw_row(Ui *u, Surf *fb, const Row *row, int y, int h, int w, int x, int cur)
{
    Settings *s = &u->set;
    ui_panel(fb, u, x, y, w, h, 16, cur);
    int cy = y + h / 2, rx = x + w - 20;
    if (row->type == RT_BTN) {
        int bid = row->id == B_SWITCH ? L_BTN_SWITCH : row->id == B_ADD ? L_BTN_ADD : row->id == B_DEL ? L_BTN_DEL : L_BTN_RESET;
        tr_draw(fb, u, row->label, x + 20, cy, WHITE, 255);
        int bw = tr_w(u, bid) + 40;
        gfx_rrect(fb, rx - bw, cy - 20, bw, 40, 20, rgb(40, 60, 130), 235);
        tr_draw(fb, u, bid, rx - bw + 20, cy, WHITE, 255);
        return;
    }
    if (row->type == RT_NAME) {
        gfx_circle(fb, x + 52, cy, 34, WHITE, 200);
        extern void ui_draw_avatar_pub(Surf *, int, int, int, int);
        ui_draw_avatar_pub(fb, x + 52, cy, 31, s->cur_user);
        char n[8]; snprintf(n, sizeof n, "%d", s->cur_user + 1);
        int nw = gfx_text_w(&font_reg, n);
        if (s->lang == 0) { gfx_text(fb, &font_reg, x + 104, cy - font_reg.line / 2 + 1, n, WHITE, 255); tr_draw(fb, u, L_PLAYER, x + 104 + nw + 8, cy, WHITE, 255); }
        else { tr_draw(fb, u, L_PLAYER, x + 104, cy, WHITE, 255); gfx_text(fb, &font_reg, x + 104 + tr_w(u, L_PLAYER) + 8, cy - font_reg.line / 2 + 1, n, WHITE, 255); }
        return;
    }
    if (row->sub >= 0) { tr_draw(fb, u, row->label, x + 20, cy - 11, WHITE, 255); tr_draw(fb, u, row->sub, x + 20, cy + 14, MUTED, 220); }
    else tr_draw(fb, u, row->label, x + 20, cy, WHITE, 255);
    int *v = fld(s, row->id);
    if (!v) return;
    if (row->type == RT_SWITCH) draw_switch(fb, rx - 54, cy, *v);
    else if (row->type == RT_SEG) {
        int wa = tr_w(u, L_LANG_AR) + 36;
        char en[] = "English"; int we = gfx_text_w(&font_med, en) + 36;
        int sx = rx - wa - we;
        gfx_rrect(fb, sx, cy - 21, wa + we, 42, 21, rgb(0, 0, 0), 80);
        int sel_x = *v == 0 ? sx : sx + wa, sel_w = *v == 0 ? wa : we;
        gfx_rrect(fb, sel_x + 2, cy - 19, sel_w - 4, 38, 19, WHITE, 255);
        tr_draw(fb, u, L_LANG_AR, sx + 18, cy, *v == 0 ? INK : MUTED, 255);
        gfx_text(fb, &font_med, sx + wa + 18, cy - font_med.line / 2 + 1, en, *v == 1 ? INK : MUTED, 255);
    } else if (row->type == RT_SWATCH) {
        int nn = row->id == F_BG ? SCENE_NBG : SCENE_NACC;
        for (int k = nn - 1; k >= 0; k--) {
            int sx = rx - 17 - (nn - 1 - k) * 46;
            uint32_t c = row->id == F_BG ? scene_swatch_bg(k) : scene_swatch_accent(k);
            gfx_rrect(fb, sx - 17, cy - 17, 34, 34, 9, c, 255);
            if (k == *v) gfx_rrect_outline(fb, sx - 21, cy - 21, 42, 42, 12, 3, WHITE, 255);
        }
    } else if (row->type == RT_SLIDER) {
        int lo, hi, st; slider_range(row->id, &lo, &hi, &st);
        int tw = 180, tx = rx - tw;
        gfx_rrect(fb, tx, cy - 3, tw, 6, 3, WHITE, 70);
        int fillw = (*v - lo) * tw / (hi - lo);
        gfx_rrect(fb, tx, cy - 3, fillw < 6 ? 6 : fillw, 6, 3, ACC, 255);
        gfx_circle(fb, tx + fillw, cy, 9, ACC, 255);
        char t[16];
        if (row->id == F_SPEED) snprintf(t, sizeof t, "%d.%02dx", *v / 100, *v % 100);
        else if (row->id == F_ICON || row->id == F_RAD) snprintf(t, sizeof t, "%dpx", *v);
        else snprintf(t, sizeof t, "%d%%", *v);
        gfx_text(fb, &font_small, tx - 14 - gfx_text_w(&font_small, t), cy - font_small.line / 2 + 1, t, MUTED, 230);
    }
}

void page_settings_draw(Ui *u, Surf *fb)
{
    if (u->spage == 0) {
        static const int cat[3] = { L_C_USER, L_C_GEN, L_C_THEME }, desc[3] = { L_C_USER_D, L_C_GEN_D, L_C_THEME_D };
        title(u, fb, L_T_SET);
        for (int i = 0; i < 3; i++) {
            int x = 64 + i * 286, y = 165, w = 270, h = 158, cx = x + 40, cy = y + 44;
            ui_panel(fb, u, x, y, w, h, 20, i == u->scat);
            uint32_t c = ACC;
            if (i == 0) { gfx_ring(fb, cx, cy - 6, 9, 3, c, 255); gfx_rrect_outline(fb, cx - 16, cy + 6, 32, 20, 9, 3, c, 255); }
            else if (i == 1) { gfx_line(fb, cx - 16, cy - 8, cx + 8, cy - 8, 3, c, 255); gfx_line(fb, cx - 16, cy + 10, cx + 16, cy + 10, 3, c, 255); gfx_ring(fb, cx + 10, cy - 8, 6, 3, c, 255); gfx_ring(fb, cx - 6, cy + 10, 6, 3, c, 255); }
            else { gfx_ring(fb, cx, cy, 17, 3, c, 255); gfx_circle(fb, cx - 7, cy - 4, 3, c, 255); gfx_circle(fb, cx, cy - 9, 3, c, 255); gfx_circle(fb, cx + 8, cy - 4, 3, c, 255); }
            tr_draw(fb, u, cat[i], x + 20, y + 104, WHITE, 255);
            tr_draw(fb, u, desc[i], x + 20, y + 134, MUTED, 220);
        }
        return;
    }
    title(u, fb, u->spage == 1 ? L_T_USER : u->spage == 2 ? L_T_GEN : L_T_THEME);
    int n; const Row *rows = page_rows(u->spage, &n);
    const int x = 64, w = 780, h = 52, gap = 8;
    int y = 165;
    for (int i = 0; i < n; i++) {
        int rh = (rows[i].sub >= 0 || rows[i].type == RT_NAME) ? 64 : h;
        if (u->spage == 1 && i == 0) rh = 80;
        if (u->spage == 1 && i == 3 && u->set.users <= 1) { continue; }
        draw_row(u, fb, &rows[i], y, rh, w, x, i == u->srow);
        y += rh + gap;
    }
}

/* ---------------- media ---------------- */
void page_media_input(Ui *u, int l, int r, int ok)
{
    if (l && u->media_sel > 0) u->media_sel--;
    if (r && u->media_sel < 3) u->media_sel++;
    if (!ok) return;
    static const char *dirs[3] = { "/dev_hdd0/video", "/dev_hdd0/music", "/dev_hdd0/photo" };
    if (u->media_sel < 3) {
        u->tab = TAB_FILES;
        files_open(&u->files, dirs[u->media_sel]);
    } else ui_show_message(u, L_M_NA);
}

/* ---------------- files ---------------- */
static void fmt_size(char *o, int n, long long s)
{
    if (s < 0) o[0] = 0;
    else if (s < 1024) snprintf(o, n, "%lld B", s);
    else if (s < 1024 * 1024) snprintf(o, n, "%.1f KB", s / 1024.0);
    else if (s < 1024LL * 1024 * 1024) snprintf(o, n, "%.1f MB", s / 1048576.0);
    else snprintf(o, n, "%.2f GB", s / 1073741824.0);
}

void page_files_draw(Ui *u, Surf *fb)
{
    Files *f = &u->files;
    title(u, fb, L_T_FILES);
    char path[300], fit[300];
    snprintf(path, sizeof path, "%s", f->cwd[0] ? f->cwd : "/");
    ui_text_fit(fit, sizeof fit, path, &font_small, 900);
    gfx_text(fb, &font_small, 66, 166, fit, ACC, 255);
    int ns = files_nsel(f);
    if (ns) { char t[24]; snprintf(t, sizeof t, "%d", ns); gfx_text(fb, &font_med, 1216 - gfx_text_w(&font_med, t), 150, t, WHITE, 255); }
    const int x = 64, w = 1000, rh = 56, stride = 64, y0 = 200, vis = 7;
    if (!f->n) { tr_draw(fb, u, L_F_EMPTY, x + 20, y0 + 30, MUTED, 230); return; }
    if (f->sel < f->top) f->top = f->sel;
    if (f->sel >= f->top + vis) f->top = f->sel - vis + 1;
    for (int i = 0; i < vis && f->top + i < f->n; i++) {
        const FileItem *it = &f->items[f->top + i];
        int y = y0 + i * stride, cur = f->top + i == f->sel, cy = y + rh / 2;
        ui_panel(fb, u, x, y, w, rh, 14, cur);
        if (f->cwd[0]) {  /* checkbox */
            gfx_rrect(fb, x + 16, cy - 13, 26, 26, 8, it->sel ? ACC : WHITE, it->sel ? 255 : 40);
            gfx_rrect_outline(fb, x + 16, cy - 13, 26, 26, 8, 2, it->sel ? ACC : rgb(220, 225, 245), 255);
            if (it->sel) { gfx_line(fb, x + 22, cy, x + 28, cy + 6, 3, INK, 255); gfx_line(fb, x + 28, cy + 6, x + 37, cy - 6, 3, INK, 255); }
        }
        int ix = x + 62;
        if (it->dir) { gfx_rrect_outline(fb, ix, cy - 10, 30, 22, 5, 3, ACC, 255); gfx_rrect(fb, ix, cy - 14, 13, 7, 3, ACC, 255); }
        else { gfx_rrect_outline(fb, ix + 4, cy - 13, 22, 27, 4, 3, MUTED, 255); }
        char nm[110]; ui_text_fit(nm, sizeof nm, it->name, &font_reg, w - 260);
        gfx_text(fb, &font_reg, ix + 50, cy - font_reg.line / 2 + 1, nm, WHITE, 255);
        char sz[24]; fmt_size(sz, sizeof sz, it->size);
        if (sz[0]) gfx_text(fb, &font_small, x + w - 22 - gfx_text_w(&font_small, sz), cy - font_small.line / 2 + 1, sz, MUTED, 230);
    }
    if (f->n > vis) {
        int th = vis * stride * vis / f->n; if (th < 30) th = 30;
        int ty = y0 + (vis * stride - th) * f->top / (f->n - vis);
        gfx_rrect(fb, x + w + 14, ty, 6, th, 3, WHITE, 140);
    }
}

/* ---------------- store ---------------- */
void page_store_draw(Ui *u, Surf *fb)
{
    Store *s = &u->store;
    title(u, fb, L_T_STORE);
    /* filter pill "All . N" */
    int fid = s->filter == 0 ? L_STORE_ALL : s->filter == 1 ? L_TAB_GAMES : L_TAB_APPS;
    char cnt[16]; snprintf(cnt, sizeof cnt, " . %d", s->nvis);
    int px = 64 + tr_w(u, L_T_STORE) + 24, pw = tr_w(u, fid) + gfx_text_w(&font_small, cnt) + 28;
    gfx_rrect(fb, px, 104, pw, 34, 17, WHITE, 40);
    tr_draw(fb, u, fid, px + 14, 121, WHITE, 255);
    gfx_text(fb, &font_small, px + 14 + tr_w(u, fid), 121 - font_small.line / 2 + 1, cnt, WHITE, 255);
    /* "[triangle] for menu" */
    int rx = SW - 64;
    tr_draw(fb, u, L_STORE_HINT, rx - tr_w(u, L_STORE_HINT), 122, MUTED, 220);
    int tx = rx - tr_w(u, L_STORE_HINT) - 22;
    gfx_line(fb, tx, 114, tx - 7, 128, 2, MUTED, 220); gfx_line(fb, tx, 114, tx + 7, 128, 2, MUTED, 220); gfx_line(fb, tx - 7, 128, tx + 7, 128, 2, MUTED, 220);
    if (!s->nvis) {
        int id = s->n ? L_NO_RESULTS : L_STORE_EMPTY;
        tr_draw(fb, u, id, (SW - tr_w(u, id)) / 2, 260, MUTED, 230);
        return;
    }
    const int cols = 4, cw = 280, ch = 205, gx = 18, gy = 20, x0 = 64, y0 = 160;
    int row = s->sel / cols, rows_vis = 2, first = row >= rows_vis ? row - rows_vis + 1 : 0;
    for (int k = first * cols; k < s->nvis && k < (first + rows_vis) * cols; k++) {
        const StoreItem *it = &s->items[s->vis[k]];
        int cx = x0 + (k % cols) * (cw + gx), cy = y0 + (k / cols - first) * (ch + gy), cur = k == s->sel;
        ui_panel(fb, u, cx, cy, cw, ch, 20, cur);
        scene_tile(fb, cx + 10, cy + 10, 110, 14, it->hash);
        char t[60]; ui_text_fit(t, sizeof t, it->name, &font_med, cw - 30);
        gfx_text(fb, &font_med, cx + 16, cy + 132, t, WHITE, 255);
        char d[100]; ui_text_fit(d, sizeof d, it->desc, &font_small, cw - 30);
        gfx_text(fb, &font_small, cx + 16, cy + 168, d, MUTED, 230);
        gfx_rrect(fb, cx + 132, cy + 14, gfx_text_w(&font_small, it->kind) + 20, 28, 8, ACC, 255);
        gfx_text(fb, &font_small, cx + 142, cy + 17, it->kind, INK, 255);
        gfx_text(fb, &font_small, cx + 134, cy + 56, it->size, MUTED, 230);
    }
}
