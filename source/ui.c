#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include "ui.h"
#include "scene.h"

#define FADE_LEN 14
#define WHITE rgb(255, 255, 255)
#define MUTED rgb(205, 210, 240)
#define INK   rgb(10, 20, 60)
#define ACC   scene_accent(0)
#define ISLIST(t) ((t) < NLISTTABS)
static const int TABLIST[NLISTTABS] = { SRC_MM, SRC_APP };
#define LST(u) (&(u)->lists[TABLIST[(u)->tab]])

static const uint32_t DEFAULT_HASH[2] = { 0x1234abcdu, 0x7a11c0deu };

/* ---------------- text helpers (Arabic bitmap labels or English font) ---------------- */
static const Font *fstyle(int s)
{
    switch (s) { case 0: return &font_small; case 1: return &font_reg; case 2: return &font_med; case 3: return &font_title; default: return &font_big; }
}
int tr_w(const Ui *u, int id) { return u->set.lang == 0 ? labels[id].w : gfx_text_w(fstyle(label_style[id]), label_en[id]); }
void tr_draw(Surf *fb, const Ui *u, int id, int x, int cy, uint32_t c, int a)
{
    if (u->set.lang == 0) gfx_label(fb, &labels[id], x, cy - labels[id].h / 2, c, a);
    else { const Font *f = fstyle(label_style[id]); gfx_text(fb, f, x, cy - f->line / 2 + 1, label_en[id], c, a); }
}

void ui_panel(Surf *fb, const Ui *u, int x, int y, int w, int h, int r, int selected)
{
    int a = 70 + u->set.panel * 170 / 100;
    gfx_rrect(fb, x, y, w, h, r, rgb(14, 10, 40), a);
    if (selected) {
        gfx_rrect(fb, x, y, w, h, r, ACC, 90);
        gfx_rrect_outline(fb, x - 1, y - 1, w + 2, h + 2, r + 1, 3, WHITE, 255);
    }
}

void ui_text_fit(char *out, int n, const char *in, const Font *f, int maxw)
{
    snprintf(out, n, "%s", in);
    if (gfx_text_w(f, out) <= maxw) return;
    int len = (int)strlen(out);
    while (len > 1) {
        out[--len] = 0;
        char tmp[200];
        snprintf(tmp, sizeof tmp, "%s...", out);
        if (gfx_text_w(f, tmp) <= maxw) { snprintf(out, n, "%s", tmp); return; }
    }
}

/* ---------------- moves between Games and Apps ---------------- */
static int moved_idx(const Ui *u, const Entry *e)
{
    for (int i = 0; i < u->nmv; i++) if (!strncmp(u->mv[i], e->path, 199)) return i;
    return -1;
}
static void save_moves(const Ui *u)
{
    char p[300]; snprintf(p, sizeof p, "%s/moves.cfg", u->data_dir);
    FILE *f = fopen(p, "w");
    if (!f) return;
    for (int i = 0; i < u->nmv; i++) fprintf(f, "%s\n", u->mv[i]);
    fclose(f);
}
static void load_moves(Ui *u)
{
    char p[300]; snprintf(p, sizeof p, "%s/moves.cfg", u->data_dir);
    FILE *f = fopen(p, "r");
    if (!f) return;
    char line[256];
    while (u->nmv < MAXMV && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0]) snprintf(u->mv[u->nmv++], 200, "%s", line);
    }
    fclose(f);
}
static int cmp_title(const void *a, const void *b) { return strcasecmp(((const Entry *)a)->title, ((const Entry *)b)->title); }

/* Rebuilds the Games / Apps lists from all entries: original place XOR moved flag. */
static void regroup(Ui *u)
{
    List *g = &u->lists[SRC_MM], *a = &u->lists[SRC_APP];
    int total = g->n + a->n;
    Entry *all = (Entry *)malloc((size_t)(total ? total : 1) * sizeof(Entry));
    Entry *ng = (Entry *)malloc((size_t)(total ? total : 1) * sizeof(Entry));
    Entry *na = (Entry *)malloc((size_t)(total ? total : 1) * sizeof(Entry));
    if (!all || !ng || !na) { free(all); free(ng); free(na); return; }
    int n = 0, cg = 0, ca = 0;
    for (int i = 0; i < g->n; i++) all[n++] = g->items[i];
    for (int i = 0; i < a->n; i++) all[n++] = a->items[i];
    for (int i = 0; i < n; i++) {
        int is_app = (all[i].src == SRC_APP) ^ (moved_idx(u, &all[i]) >= 0);
        if (is_app) na[ca++] = all[i]; else ng[cg++] = all[i];
    }
    qsort(ng, cg, sizeof(Entry), cmp_title);
    qsort(na, ca, sizeof(Entry), cmp_title);
    free(g->items); free(a->items); free(all);
    g->items = ng; g->n = cg; g->cap = total ? total : 1;
    a->items = na; a->n = ca; a->cap = total ? total : 1;
}

/* ---------------- init / reset ---------------- */
void ui_apply_theme(Ui *u)
{
    scene_set_theme(u->set.bg, u->set.accent, u->set.waves);
    for (int i = 0; i < 2; i++) if (u->scn[i].px) scene_render(&u->scn[i], u->hash[i], 1);
}

int ui_init(Ui *u, List *lists, const char *data_dir)
{
    memset(u, 0, sizeof *u);
    u->lists = lists;
    snprintf(u->data_dir, sizeof u->data_dir, "%s", data_dir);
    char p[300];
    snprintf(p, sizeof p, "%s/settings.cfg", data_dir);
    settings_load(&u->set, p);
    load_moves(u);
    scene_set_theme(u->set.bg, u->set.accent, u->set.waves);
    snprintf(p, sizeof p, "%s/store.txt", data_dir);
    store_load(&u->store, p);
    files_open(&u->files, "");
    for (int i = 0; i < 2; i++) {
        u->scn[i].w = SW; u->scn[i].h = SH;
        u->scn[i].px = (uint32_t *)malloc((size_t)SW * SH * 4);
        if (!u->scn[i].px) return 0;
    }
    u->mode = M_SCANNING;
    u->want = u->hash[0] = DEFAULT_HASH[0];
    scene_render(&u->scn[0], u->hash[0], 1);
    u->tabX = 64; u->tabW = 100;
    u->login_sel = u->set.cur_user;
    return 1;
}

void ui_free(Ui *u)
{
    files_free(&u->files); store_free(&u->store);
    for (int i = 0; i < 2; i++) { free(u->scn[i].px); u->scn[i].px = NULL; }
}

static const Entry *cur_entry(const Ui *u)
{
    if (!ISLIST(u->tab)) return NULL;
    const List *l = LST(u);
    if (!l->n) return NULL;
    int i = u->sel[u->tab];
    if (i >= l->n) i = l->n - 1;
    return &l->items[i];
}

void ui_reset(Ui *u)
{
    regroup(u);
    u->tab = 0; u->selPos = 0; u->heroT = 0;
    u->sel[0] = u->sel[1] = 0;
    const Entry *e = cur_entry(u);
    u->want = u->hash[u->cur] = e ? e->hash : DEFAULT_HASH[0];
    u->fade = 0; u->settle = 0;
    scene_render(&u->scn[u->cur], u->want, 1);
}

void ui_set_mode(Ui *u, Mode m) { u->mode = m; u->dlgT = 0; }
void ui_show_message(Ui *u, int label_id) { u->msg = label_id; u->mode = M_MSG; u->dlgT = 0; }

static void retarget(Ui *u)
{
    const Entry *e = cur_entry(u);
    u->want = e ? e->hash : DEFAULT_HASH[ISLIST(u->tab) ? u->tab : 0];
    u->settle = 0;
    u->heroT = 0;
}

static int pressed(Ui *u, int idx, int now)
{
    if (!now) { u->rep[idx] = 0; return 0; }
    int r = u->rep[idx]++;
    return r == 0 || (r > 16 && (r - 16) % 5 == 0);
}

static void save_settings(const Ui *u)
{
    char p[300]; snprintf(p, sizeof p, "%s/settings.cfg", u->data_dir);
    settings_save(&u->set, p);
}

static void open_popup(Ui *u, int kind)
{
    u->pop.kind = kind; u->pop.cur = 0;
    if (kind == POP_MOVE) { u->pop.n = 1; u->pop.ids[0] = u->tab == TAB_GAMES ? L_MV_APPS : L_MV_GAMES; }
    else if (kind == POP_FILES) { u->pop.n = 3; u->pop.ids[0] = L_F_DELETE; u->pop.ids[1] = L_F_SELALL; u->pop.ids[2] = L_F_CLR; }
    else { u->pop.n = 3; u->pop.ids[0] = L_STORE_ALL; u->pop.ids[1] = L_TAB_GAMES; u->pop.ids[2] = L_TAB_APPS; u->pop.cur = u->store.filter; }
    u->mode = M_POPUP; u->dlgT = 0;
}

static void popup_apply(Ui *u)
{
    int c = u->pop.cur;
    u->mode = M_BROWSE;
    if (u->pop.kind == POP_MOVE) {
        const Entry *e = cur_entry(u);
        if (!e) return;
        int i = moved_idx(u, e);
        if (i >= 0) { memmove(u->mv[i], u->mv[i + 1], (size_t)(u->nmv - i - 1) * 200); u->nmv--; }
        else if (u->nmv < MAXMV) snprintf(u->mv[u->nmv++], 200, "%s", e->path);
        save_moves(u);
        regroup(u);
        List *l = LST(u);
        if (u->sel[u->tab] >= l->n) u->sel[u->tab] = l->n ? l->n - 1 : 0;
        u->selPos = (float)u->sel[u->tab];
        retarget(u);
    } else if (u->pop.kind == POP_FILES) {
        if (c == 0) { if (files_cur(&u->files) && u->files.cwd[0]) { u->mode = M_DELCONFIRM; u->dlgT = 0; } }
        else files_select_all(&u->files, c == 1);
    } else {
        u->store.filter = c; store_refilter(&u->store);
    }
}

UiAction ui_update(Ui *u, const Pad *p)
{
    UiAction act = UA_NONE;
    u->frame++;
    u->phase += u->set.speed / 100.0f;
    int ok = p->circle && !u->prev.circle, back = p->cross && !u->prev.cross;
    int sq = p->square && !u->prev.square, tri = p->tri && !u->prev.tri;
    int l = pressed(u, 0, p->left), r = pressed(u, 1, p->right), up = pressed(u, 2, p->up), dn = pressed(u, 3, p->down);
    int pl1 = p->l1 && !u->prev.l1, pr1 = p->r1 && !u->prev.r1;
    u->prev = *p;
    Settings old = u->set;

    switch (u->mode) {
    case M_LOGIN: {
        int n = u->set.users + (u->set.users < MAXUSERS ? 1 : 0);
        if (l && u->login_sel > 0) u->login_sel--;
        if (r && u->login_sel < n - 1) u->login_sel++;
        if (ok) {
            if (u->login_sel >= u->set.users) { u->set.users++; u->set.cur_user = u->set.users - 1; }
            else u->set.cur_user = u->login_sel;
            u->mode = M_BROWSE; u->dlgT = 0;
        }
        break; }
    case M_BROWSE: {
        int ntab = 0, on_page = !ISLIST(u->tab);
        if (pl1 || (!on_page && up)) ntab = -1;
        if (pr1 || (!on_page && dn)) ntab = 1;
        if (ntab) {
            u->tab = (u->tab + NTABS + ntab) % NTABS;
            if (ISLIST(u->tab)) u->selPos = (float)u->sel[u->tab];
            if (u->tab == TAB_SET) { u->spage = 0; u->srow = 0; }
            retarget(u);
        } else if (ISLIST(u->tab)) {
            List *ls = LST(u);
            if (ls->n) {
                int s = u->sel[u->tab];
                if (l && s > 0) s--;
                if (r && s < ls->n - 1) s++;
                if (s != u->sel[u->tab]) { u->sel[u->tab] = s; retarget(u); }
            }
            if (ok && ls->n) {
                if (u->set.askplay) { u->mode = M_PLAYCONFIRM; u->dlgT = 0; }
                else { u->mode = M_LAUNCHING; u->launch_t = 0; u->launch_entry = cur_entry(u); }
            } else if (tri && ls->n) open_popup(u, POP_MOVE);
            else if (back) { u->mode = M_EXITCONFIRM; u->dlgT = 0; }
        } else if (u->tab == TAB_MEDIA) {
            page_media_input(u, l, r, ok);
            if (back) { u->mode = M_EXITCONFIRM; u->dlgT = 0; }
        } else if (u->tab == TAB_SET) {
            int was = u->spage;
            page_settings_input(u, l, r, up, dn, ok, back);
            if (back && was == 0) { u->mode = M_EXITCONFIRM; u->dlgT = 0; }
        } else if (u->tab == TAB_FILES) {
            Files *f = &u->files;
            if (up && f->sel > 0) f->sel--;
            if (dn && f->sel < f->n - 1) f->sel++;
            if (l && f->sel > 0) f->sel = f->sel > 5 ? f->sel - 5 : 0;
            if (r && f->sel < f->n - 1) f->sel = f->sel + 5 < f->n ? f->sel + 5 : f->n - 1;
            if (ok) files_enter(f);
            else if (sq && files_cur(f) && f->cwd[0]) f->items[f->sel].sel ^= 1;
            else if (tri) open_popup(u, POP_FILES);
            else if (back) { if (!files_up(f)) { u->mode = M_EXITCONFIRM; u->dlgT = 0; } }
        } else { /* store */
            Store *st = &u->store;
            if (l && st->sel > 0) st->sel--;
            if (r && st->sel < st->nvis - 1) st->sel++;
            if (up && st->sel >= 4) st->sel -= 4;
            if (dn && st->sel + 4 < st->nvis) st->sel += 4;
            if (ok && st->nvis) ui_show_message(u, L_STORE_NA);
            else if (tri) open_popup(u, POP_STORE);
            else if (back) { u->mode = M_EXITCONFIRM; u->dlgT = 0; }
        }
        break; }
    case M_POPUP:
        if (up && u->pop.cur > 0) u->pop.cur--;
        if (dn && u->pop.cur < u->pop.n - 1) u->pop.cur++;
        if (ok) popup_apply(u);
        else if (back || tri) u->mode = M_BROWSE;
        break;
    case M_PLAYCONFIRM:
        if (ok) { u->mode = M_LAUNCHING; u->launch_t = 0; u->launch_entry = cur_entry(u); }
        else if (back) u->mode = M_BROWSE;
        break;
    case M_EXITCONFIRM:
        if (ok) act = UA_EXIT;
        else if (back) u->mode = M_BROWSE;
        break;
    case M_DELCONFIRM:
        if (ok) { int fails = files_delete(&u->files); u->mode = M_BROWSE; if (fails) ui_show_message(u, L_F_DEL_FAIL); }
        else if (back) u->mode = M_BROWSE;
        break;
    case M_LAUNCHING:
        if (++u->launch_t == 26) act = UA_LAUNCH;
        break;
    case M_MSG:
        if (ok || back) u->mode = M_BROWSE;
        break;
    case M_SCANNING: break;
    }

    if (memcmp(&old, &u->set, sizeof old)) {
        if (old.bg != u->set.bg || old.accent != u->set.accent || old.waves != u->set.waves) ui_apply_theme(u);
        save_settings(u);
    }

    /* animation state */
    if (ISLIST(u->tab)) {
        float d = (float)u->sel[u->tab] - u->selPos;
        u->selPos += d * 0.30f;
        if (fabsf(d) < 0.01f) u->selPos = (float)u->sel[u->tab];
    }
    if (u->heroT < 12) u->heroT++;
    if (u->mode == M_PLAYCONFIRM || u->mode == M_EXITCONFIRM || u->mode == M_MSG || u->mode == M_DELCONFIRM || u->mode == M_POPUP || u->mode == M_LOGIN) { if (u->dlgT < 8) u->dlgT++; }
    else if (u->dlgT > 0) u->dlgT--;

    u->settle++;
    if (u->fade) {
        if (++u->fade > FADE_LEN) { u->cur ^= 1; u->fade = 0; }
    } else if (u->settle >= 6 && u->want != u->hash[u->cur]) {
        int o = u->cur ^ 1;
        u->hash[o] = u->want;
        scene_render(&u->scn[o], u->want, 1);
        u->fade = 1;
    }
    return act;
}

/* ---------------- drawing ---------------- */
static void text_shadow(Surf *s, const Font *f, int x, int y, const char *t, uint32_t c, int a)
{
    gfx_text(s, f, x + 2, y + 3, t, rgb(0, 0, 0), a * 110 / 255);
    gfx_text(s, f, x, y, t, c, a);
}

static void spinner(Surf *s, int cx, int cy, int frame, int a)
{
    for (int i = 0; i < 10; i++) {
        float ang = i * 6.2831853f / 10;
        int fade = 255 - ((frame / 2 + 10 - i) % 10) * 24;
        gfx_circle(s, cx + (int)(cosf(ang) * 30), cy + (int)(sinf(ang) * 30), 5, WHITE, fade * a / 255 > 0 ? fade * a / 255 : 0);
    }
}

static void user_name(const Ui *u, Surf *fb, int idx, int cx, int y, uint32_t c, int a)
{
    char n[8]; snprintf(n, sizeof n, "%d", idx + 1);
    int nw = gfx_text_w(&font_reg, n), lw = tr_w(u, L_PLAYER), tot = nw + 8 + lw, x = cx - tot / 2;
    if (u->set.lang == 0) { gfx_text(fb, &font_reg, x, y - font_reg.line / 2 + 1, n, c, a); tr_draw(fb, u, L_PLAYER, x + nw + 8, y, c, a); }
    else { tr_draw(fb, u, L_PLAYER, x, y, c, a); gfx_text(fb, &font_reg, x + lw + 8, y - font_reg.line / 2 + 1, n, c, a); }
}

static void avatar(Surf *fb, int cx, int cy, int r, int idx, int a)
{
    static const uint32_t cols[MAXUSERS] = { 0x4a7bd6, 0x8a62d8, 0x2fa79a, 0xd0678a };
    gfx_circle(fb, cx, cy, r, rgb(cols[idx & 3] >> 16, (cols[idx & 3] >> 8) & 255, cols[idx & 3] & 255), a);
    gfx_circle(fb, cx, cy - r / 4, r / 3, rgb(235, 240, 255), a);
    gfx_rrect(fb, cx - r * 55 / 100, cy + r / 8, r * 110 / 100, r * 55 / 100, r / 3, rgb(235, 240, 255), a);
}

void ui_draw_avatar_pub(Surf *fb, int cx, int cy, int r, int idx) { avatar(fb, cx, cy, r, idx, 255); }

static void draw_topbar(Ui *u, Surf *fb)
{
    static const int ids[NTABS] = { L_TAB_GAMES, L_TAB_APPS, L_TAB_MEDIA, L_TAB_SET, L_TAB_STORE, L_TAB_FILES };
    int cy = 48, x = 64;
    gfx_rrect(fb, x, cy - 17, 48, 34, 10, WHITE, 36);
    gfx_rrect_outline(fb, x, cy - 17, 48, 34, 10, 2, WHITE, 150);
    gfx_text(fb, &font_small, x + 12, cy - 12, "L1", WHITE, 255);
    x += 48 + 28;
    int ax = 0, aw = 0;
    for (int t = 0; t < NTABS; t++) {
        int w = tr_w(u, ids[t]);
        uint32_t c = t == u->tab ? WHITE : MUTED;
        tr_draw(fb, u, ids[t], x, cy, c, t == u->tab ? 255 : 190);
        if (t == u->tab) { ax = x; aw = w; }
        x += w + 32;
    }
    gfx_rrect(fb, x - 8, cy - 17, 48, 34, 10, WHITE, 36);
    gfx_rrect_outline(fb, x - 8, cy - 17, 48, 34, 10, 2, WHITE, 150);
    gfx_text(fb, &font_small, x + 4, cy - 12, "R1", WHITE, 255);
    u->tabX += ((float)ax - u->tabX) * 0.3f;
    u->tabW += ((float)aw - u->tabW) * 0.3f;
    gfx_rrect(fb, (int)u->tabX, cy + 24, (int)u->tabW, 4, 2, ACC, 255);

    char ck[16];
    page_clock(ck, sizeof ck, u->set.h24);
    int rx = SW - 64;
    gfx_circle(fb, rx - 22, cy, 24, WHITE, 200);
    avatar(fb, rx - 22, cy, 21, u->set.cur_user, 255);
    int cw = gfx_text_w(&font_reg, ck);
    gfx_text(fb, &font_reg, rx - 62 - cw, cy - font_reg.line / 2, ck, WHITE, 255);
}

static void hint_icon(Surf *fb, int kind, int cx, int cy)
{
    const int r = 19;
    gfx_circle(fb, cx, cy, r, WHITE, 36);
    gfx_ring(fb, cx, cy, r, 2, rgb(150, 160, 200), 200);
    if (kind == 0) gfx_ring(fb, cx, cy, 8, 3, rgb(240, 100, 110), 255);                       /* O */
    else if (kind == 1) { gfx_line(fb, cx - 7, cy - 7, cx + 7, cy + 7, 3, rgb(120, 170, 255), 255); gfx_line(fb, cx - 7, cy + 7, cx + 7, cy - 7, 3, rgb(120, 170, 255), 255); }
    else if (kind == 2) { /* triangle outline */
        gfx_line(fb, cx, cy - 9, cx - 9, cy + 7, 3, rgb(80, 220, 180), 255);
        gfx_line(fb, cx, cy - 9, cx + 9, cy + 7, 3, rgb(80, 220, 180), 255);
        gfx_line(fb, cx - 9, cy + 7, cx + 9, cy + 7, 3, rgb(80, 220, 180), 255);
    } else if (kind == 3) gfx_rrect_outline(fb, cx - 8, cy - 8, 16, 16, 2, 3, rgb(255, 140, 190), 255);   /* square */
    else { /* arrows: 4=left 5=up 6=down 7=right */
        int dx = kind == 4 ? -1 : kind == 7 ? 1 : 0, dy = kind == 5 ? -1 : kind == 6 ? 1 : 0;
        int px = -dy, py = dx;
        int tx = cx + dx * 6, ty = cy + dy * 6;
        gfx_line(fb, tx, ty, cx - dx * 3 + px * 7, cy - dy * 3 + py * 7, 3, WHITE, 230);
        gfx_line(fb, tx, ty, cx - dx * 3 - px * 7, cy - dy * 3 - py * 7, 3, WHITE, 230);
    }
}

static void draw_hints(Ui *u, Surf *fb)
{
    int cy = 678;
    int x = 64;
    /* d-pad group */
    static const int order[4] = { 4, 5, 6, 7 };
    for (int i = 0; i < 4; i++) { hint_icon(fb, order[i], x + 19, cy); x += 46; }
    tr_draw(fb, u, L_H_NAV, x + 4, cy, MUTED, 230);

    int kinds[4], ids[4], n = 0;
    if (u->tab == TAB_FILES) { kinds[n] = 3; ids[n++] = L_H_SEL; }
    int menu = (ISLIST(u->tab) && LST(u)->n) || u->tab == TAB_FILES || u->tab == TAB_STORE;
    if (menu) { kinds[n] = 2; ids[n++] = L_H_MENU; }
    kinds[n] = 1; ids[n++] = L_H_BACK;
    kinds[n] = 0; ids[n++] = L_H_OK;
    int tot = 0;
    for (int i = 0; i < n; i++) tot += 38 + 10 + tr_w(u, ids[i]) + (i ? 34 : 0);
    int rx = SW - 64 - tot;
    for (int i = 0; i < n; i++) {
        if (i) rx += 34;
        hint_icon(fb, kinds[i], rx + 19, cy);
        tr_draw(fb, u, ids[i], rx + 48, cy, MUTED, 230);
        rx += 38 + 10 + tr_w(u, ids[i]);
    }
}

static void draw_badge(const Ui *u, Surf *fb, int x, int y, Source s)
{
    int w, h = 30;
    char t[8] = "";
    if (s == SRC_MM) w = tr_w(u, L_B_MM);
    else if (s == SRC_APP) w = tr_w(u, L_B_APP);
    else { snprintf(t, sizeof t, "%s", s == SRC_ISO ? "ISO" : "PKG"); w = gfx_text_w(&font_small, t); }
    gfx_rrect(fb, x, y, w + 22, h, 7, WHITE, 255);
    if (s == SRC_MM) tr_draw(fb, u, L_B_MM, x + 11, y + h / 2, INK, 255);
    else if (s == SRC_APP) tr_draw(fb, u, L_B_APP, x + 11, y + h / 2, INK, 255);
    else gfx_text(fb, &font_small, x + 11, y + 3, t, INK, 255);
}
static int badge_w(const Ui *u, Source s)
{
    if (s == SRC_MM) return tr_w(u, L_B_MM) + 22;
    if (s == SRC_APP) return tr_w(u, L_B_APP) + 22;
    return gfx_text_w(&font_small, "ISO") + 22;
}

static void draw_row(Ui *u, Surf *fb)
{
    const List *l = LST(u);
    const int small = u->set.iconsz, big = small * 144 / 84, gap = 16, x0 = 64, y0 = 102;
    int rad = u->set.iconrad * small / 84;
    float sp = u->selPos;
    int fl = (int)floorf(sp);
    float scroll = fmaxf(0.f, sp - 1.f) * (small + gap);
    int from = fl - 4 < 0 ? 0 : fl - 4, to = fl + 16 > l->n - 1 ? l->n - 1 : fl + 16;
    for (int i = from; i <= to; i++) {
        float extra = 0;
        for (int j = fl; j <= fl + 1; j++) {
            if (j < 0 || j >= i) continue;
            float tj = 1.f - fabsf((float)j - sp);
            if (tj > 0) extra += (big - small) * tj;
        }
        float ti = 1.f - fabsf((float)i - sp);
        if (ti < 0) ti = 0;
        int size = small + (int)((big - small) * ti);
        int x = x0 + (int)(i * (small + gap) + extra - scroll);
        int y = y0 + (big - size) / 2;
        if (x + size < 0 || x > SW) continue;
        if (ti > 0.05f) {
            for (int g = 3; g >= 1; g--)
                gfx_rrect(fb, x - 3 - g * 6, y - 3 - g * 6, size + 6 + g * 12, size + 6 + g * 12, rad + 3 + g * 6, ACC, (int)(22 * ti));
            gfx_rrect(fb, x - 3, y - 3, size + 6, size + 6, rad + 3, WHITE, (int)(255 * ti));
        }
        scene_tile(fb, x, y, size, rad, l->items[i].hash);
    }
    const Entry *e = cur_entry(u);
    if (e) {
        int a = u->heroT * 255 / 12;
        int ny = y0 + big + 20;
        draw_badge(u, fb, 64, ny, e->src);
        char t[100];
        ui_text_fit(t, sizeof t, e->title, &font_med, 760);
        gfx_text(fb, &font_med, 64 + badge_w(u, e->src) + 14, ny - 2, t, WHITE, a);
    }
    char n[16];
    snprintf(n, sizeof n, "%d", l->n);
    int lw = tr_w(u, L_LBL_ITEMS), nw = gfx_text_w(&font_med, n), rx = SW - 64;
    tr_draw(fb, u, L_LBL_ITEMS, rx - lw, y0 + big + 34, MUTED, 230);
    gfx_text(fb, &font_med, rx - lw - 10 - nw, y0 + big + 18, n, WHITE, 255);
}

static void draw_hero(Ui *u, Surf *fb)
{
    const Entry *e = cur_entry(u);
    if (!e) return;
    int a = u->heroT * 255 / 12, slide = (12 - u->heroT) * 2;
    char t[100];
    const Font *f = &font_big;
    snprintf(t, sizeof t, "%s", e->title);
    if (gfx_text_w(f, t) > 820) f = &font_med;
    ui_text_fit(t, sizeof t, e->title, f, 820);
    text_shadow(fb, f, 64, 432 + slide, t, WHITE, a);
    char info[320], fit[320];
    snprintf(info, sizeof info, "%s%s%s", e->id, e->id[0] ? "   |   " : "", e->path);
    ui_text_fit(fit, sizeof fit, info, &font_small, 900);
    gfx_text(fb, &font_small, 66, 510 + slide, fit, MUTED, a * 220 / 255);

    int bx = 64, by = 560, bw = 250, bh = 62;
    gfx_rrect(fb, bx + 4, by + 8, bw, bh, 31, rgb(0, 0, 0), 70);
    gfx_rrect(fb, bx, by, bw, bh, 31, WHITE, 255);
    gfx_tri(fb, bx + 34, by + 20, bx + 34, by + 42, bx + 52, by + 31, INK, 255);
    tr_draw(fb, u, e->src == SRC_APP ? L_BTN_OPEN : L_BTN_PLAY, bx + 74, by + bh / 2, INK, 255);
    /* round "more" button (Triangle) */
    gfx_circle(fb, bx + bw + 14 + 31, by + 31, 31, rgb(14, 10, 40), 170);
    for (int i = -1; i <= 1; i++) gfx_circle(fb, bx + bw + 14 + 31 + i * 16, by + 31, 4, WHITE, 255);
}

static void draw_media(Ui *u, Surf *fb)
{
    static const int ids[4] = { L_M_VIDEO, L_M_MUSIC, L_M_PHOTOS, L_M_BROWSER };
    tr_draw(fb, u, L_T_MEDIA, 64, 128, WHITE, 255);
    for (int i = 0; i < 4; i++) {
        int x = 64 + i * 224, y = 165, w = 208, h = 122, cx = x + 36, cy = y + 36;
        ui_panel(fb, u, x, y, w, h, 20, i == u->media_sel);
        uint32_t c = ACC;
        if (i == 0) { gfx_rrect_outline(fb, cx - 16, cy - 12, 32, 24, 5, 3, c, 255); gfx_tri(fb, cx - 4, cy - 6, cx - 4, cy + 6, cx + 7, cy, c, 255); }
        else if (i == 1) { gfx_line(fb, cx - 4, cy + 10, cx - 4, cy - 12, 3, c, 255); gfx_line(fb, cx - 4, cy - 12, cx + 10, cy - 15, 3, c, 255); gfx_line(fb, cx + 10, cy - 15, cx + 10, cy + 6, 3, c, 255); gfx_circle(fb, cx - 9, cy + 11, 5, c, 255); gfx_circle(fb, cx + 5, cy + 8, 5, c, 255); }
        else if (i == 2) { gfx_rrect_outline(fb, cx - 16, cy - 14, 32, 28, 6, 3, c, 255); gfx_circle(fb, cx - 5, cy - 4, 3, c, 255); gfx_line(fb, cx - 14, cy + 11, cx - 2, cy, 3, c, 255); gfx_line(fb, cx - 2, cy, cx + 14, cy + 10, 3, c, 255); }
        else { gfx_ring(fb, cx, cy, 15, 3, c, 255); gfx_line(fb, cx - 15, cy, cx + 15, cy, 2, c, 255); gfx_line(fb, cx, cy - 15, cx, cy + 15, 2, c, 255); }
        tr_draw(fb, u, ids[i], x + 18, y + h - 30, WHITE, 255);
    }
}

static void draw_dialog(Ui *u, Surf *fb)
{
    if (u->dlgT <= 0) return;
    int a = u->dlgT * 255 / 8;
    if (u->mode == M_LOGIN) {
        gfx_rect(fb, 0, 0, SW, SH, rgb(2, 5, 20), 200 * a / 255);
        tr_draw(fb, u, L_WHO, (SW - tr_w(u, L_WHO)) / 2, 175, WHITE, a);
        int n = u->set.users + (u->set.users < MAXUSERS ? 1 : 0);
        int slot = 170, x0 = (SW - n * slot) / 2 + slot / 2, cy = 340;
        for (int i = 0; i < n; i++) {
            int cx = x0 + i * slot, sel = i == u->login_sel, r = sel ? 80 : 64;
            if (sel) { for (int g = 3; g >= 1; g--) gfx_circle(fb, cx, cy, r + 4 + g * 8, ACC, 18 * a / 255); gfx_circle(fb, cx, cy, r + 4, WHITE, a); }
            if (i < u->set.users) { avatar(fb, cx, cy, r, i, a); user_name(u, fb, i, cx, cy + r + 36, WHITE, a * (sel ? 255 : 170) / 255); }
            else {
                gfx_ring(fb, cx, cy, r, 3, rgb(210, 215, 240), a * 200 / 255);
                gfx_rect(fb, cx - 16, cy - 2, 32, 4, WHITE, a); gfx_rect(fb, cx - 2, cy - 16, 4, 32, WHITE, a);
                tr_draw(fb, u, L_ADDUSER, cx - tr_w(u, L_ADDUSER) / 2, cy + r + 36, MUTED, a * (sel ? 255 : 170) / 255);
            }
        }
        return;
    }
    if (u->mode == M_POPUP) {
        gfx_rect(fb, 0, 0, SW, SH, rgb(4, 2, 14), 120 * a / 255);
        int w = 460, rowh = 62, h = u->pop.n * rowh + 24, x = (SW - w) / 2, y = (SH - h) / 2;
        gfx_rrect(fb, x, y, w, h, 22, rgb(14, 22, 64), 245 * a / 255);
        for (int i = 0; i < u->pop.n; i++) {
            int ry = y + 12 + i * rowh;
            if (i == u->pop.cur) { gfx_rrect(fb, x + 10, ry, w - 20, rowh - 6, 14, ACC, 110 * a / 255); gfx_rrect_outline(fb, x + 10, ry, w - 20, rowh - 6, 14, 3, WHITE, a); }
            tr_draw(fb, u, u->pop.ids[i], x + 34, ry + (rowh - 6) / 2, WHITE, a);
            if (u->pop.kind == POP_STORE && i == u->store.filter) gfx_circle(fb, x + w - 36, ry + (rowh - 6) / 2, 6, ACC, a);
        }
        return;
    }
    gfx_rect(fb, 0, 0, SW, SH, rgb(4, 2, 14), 170 * a / 255);
    int bw = 700, bh = 270, bx = (SW - bw) / 2, by = (SH - bh) / 2 + (8 - u->dlgT) * 3;
    gfx_rrect(fb, bx, by, bw, bh, 22, rgb(14, 22, 64), 245 * a / 255);
    gfx_rect(fb, bx + 22, by, bw - 44, 4, ACC, a);
    int tid = u->mode == M_EXITCONFIRM ? L_DLG_EXIT : u->mode == M_DELCONFIRM ? L_F_DEL_Q : L_DLG_PLAY;
    if (u->mode == M_MSG) tr_draw(fb, u, u->msg, (SW - tr_w(u, u->msg)) / 2, by + 100, WHITE, a);
    else {
        tr_draw(fb, u, tid, (SW - tr_w(u, tid)) / 2, by + 70, WHITE, a);
        char t[100] = "";
        if (u->mode == M_PLAYCONFIRM) { const Entry *e = cur_entry(u); if (e) ui_text_fit(t, sizeof t, e->title, &font_med, bw - 80); }
        else if (u->mode == M_DELCONFIRM) {
            int ns = files_nsel(&u->files);
            if (ns > 0) snprintf(t, sizeof t, "%d", ns);
            else { const FileItem *fi = files_cur(&u->files); if (fi) ui_text_fit(t, sizeof t, fi->name, &font_med, bw - 80); }
        }
        if (t[0]) gfx_text(fb, &font_med, (SW - gfx_text_w(&font_med, t)) / 2, by + 112, t, ACC, a);
    }
    int cy = by + bh - 58;
    if (u->mode == M_MSG) {
        int w = 38 + 10 + tr_w(u, L_H_OK), x = (SW - w) / 2;
        hint_icon(fb, 0, x + 19, cy); tr_draw(fb, u, L_H_OK, x + 48, cy, WHITE, a);
    } else {
        int w1 = 48 + tr_w(u, L_H_OK), w2 = 48 + tr_w(u, L_H_BACK), gap = 70, x = (SW - (w1 + gap + w2)) / 2;
        hint_icon(fb, 0, x + 19, cy); tr_draw(fb, u, L_H_OK, x + 48, cy, WHITE, a);
        x += w1 + gap;
        hint_icon(fb, 1, x + 19, cy); tr_draw(fb, u, L_H_BACK, x + 48, cy, WHITE, a);
    }
}

void ui_draw(Ui *u, Surf *fb)
{
    if (u->fade) {
        int t = u->fade * 256 / FADE_LEN;
        gfx_crossfade(fb, &u->scn[u->cur], &u->scn[u->cur ^ 1], t > 256 ? 256 : t);
    } else gfx_copy(fb, &u->scn[u->cur]);

    if (u->set.anim) scene_overlay(fb, 0, 1, u->phase);
    if (u->set.dim) gfx_rect(fb, 0, 0, SW, SH, rgb(0, 0, 0), u->set.dim * 255 / 100);

    draw_topbar(u, fb);
    if (u->mode == M_SCANNING) {
        tr_draw(fb, u, L_SCANNING, (SW - tr_w(u, L_SCANNING)) / 2, SH / 2 + 60, WHITE, 255);
        spinner(fb, SW / 2, SH / 2 - 20, u->frame, 255);
        return;
    }
    if (ISLIST(u->tab)) {
        if (LST(u)->n == 0) {
            int id = u->tab == TAB_GAMES ? L_NO_GAMES : L_NO_APPS;
            tr_draw(fb, u, id, (SW - tr_w(u, id)) / 2, SH / 2 - 20, WHITE, 255);
            tr_draw(fb, u, L_EMPTY_HINT, (SW - tr_w(u, L_EMPTY_HINT)) / 2, SH / 2 + 34, MUTED, 230);
        } else { draw_row(u, fb); draw_hero(u, fb); }
    } else if (u->tab == TAB_MEDIA) draw_media(u, fb);
    else if (u->tab == TAB_SET) page_settings_draw(u, fb);
    else if (u->tab == TAB_FILES) page_files_draw(u, fb);
    else page_store_draw(u, fb);
    draw_hints(u, fb);
    draw_dialog(u, fb);
    if (u->mode == M_LAUNCHING) {
        int a = u->launch_t * 255 / 12; if (a > 235) a = 235;
        gfx_rect(fb, 0, 0, SW, SH, rgb(8, 6, 22), a);
        spinner(fb, SW / 2, SH / 2 - 30, u->frame, a);
        tr_draw(fb, u, L_LAUNCHING, (SW - tr_w(u, L_LAUNCHING)) / 2, SH / 2 + 50, WHITE, a);
    }
}
