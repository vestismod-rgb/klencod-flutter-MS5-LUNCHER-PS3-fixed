#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "scan.h"

#define SELF_ID "MS5LNCHR1" /* this launcher; keep in sync with APPID in the Makefile */

static char g_root[200] = "";

void scan_set_root(const char *root) { snprintf(g_root, sizeof g_root, "%s", root ? root : ""); }
const char *scan_root(void) { return g_root; }

static void full(char *out, size_t n, const char *path) { snprintf(out, n, "%s%s", g_root, path); }

static uint32_t fnv(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
    return h;
}

static int ci_ends(const char *s, const char *suf)
{
    size_t a = strlen(s), b = strlen(suf);
    if (a < b) return 0;
    for (size_t i = 0; i < b; i++)
        if (tolower((unsigned char)s[a - b + i]) != tolower((unsigned char)suf[i])) return 0;
    return 1;
}

/* ---------- PARAM.SFO ---------- */
static uint32_t le32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint32_t le16(const uint8_t *p) { return p[0] | (p[1] << 8); }

int sfo_parse(const uint8_t *buf, int len, char *title, int tn, char *id, int in)
{
    if (len < 20 || le32(buf) != 0x46535000u) return 0; /* "\0PSF" */
    uint32_t keys = le32(buf + 8), data = le32(buf + 12), count = le32(buf + 16);
    int found = 0;
    if (title && tn) title[0] = 0;
    if (id && in) id[0] = 0;
    for (uint32_t i = 0; i < count && 20 + (i + 1) * 16 <= (uint32_t)len; i++) {
        const uint8_t *e = buf + 20 + i * 16;
        uint32_t koff = keys + le16(e), dlen = le32(e + 4), doff = data + le32(e + 12);
        if (koff >= (uint32_t)len || doff + dlen > (uint32_t)len) continue;
        const char *key = (const char *)buf + koff;
        const char *val = (const char *)buf + doff;
        if (!strcmp(key, "TITLE") && title && !title[0]) {
            snprintf(title, tn, "%.*s", (int)dlen, val); found = 1;
        } else if (!strcmp(key, "TITLE_ID") && id) {
            snprintf(id, in, "%.*s", (int)dlen, val); found = 1;
        }
    }
    return found;
}

static int sfo_from_file(const char *path, char *title, int tn, char *id, int in)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t buf[8192];
    int n = (int)fread(buf, 1, sizeof buf, f);
    fclose(f);
    return sfo_parse(buf, n, title, tn, id, in);
}

/* ---------- ISO9660 ---------- */
#define SECT 2048

static int rd(FILE *f, uint32_t lba, uint32_t off, void *dst, size_t n)
{
    if (fseek(f, (long)lba * SECT + (long)off, SEEK_SET)) return 0;
    return fread(dst, 1, n, f) == n;
}

/* looks up `name` (case-insensitive, ";1" stripped) inside directory extent. */
static int dir_find(FILE *f, uint32_t lba, uint32_t size, const char *name, uint32_t *out_lba, uint32_t *out_size, int *is_dir)
{
    if (size > 256 * 1024) size = 256 * 1024;
    uint8_t *buf = (uint8_t *)malloc(size ? size : 1);
    if (!buf) return 0;
    if (!rd(f, lba, 0, buf, size)) { free(buf); return 0; }
    uint32_t pos = 0;
    int ok = 0;
    while (pos < size) {
        uint8_t len = buf[pos];
        if (len == 0) { pos = (pos / SECT + 1) * SECT; continue; }
        if (pos + len > size) break;
        uint8_t nl = buf[pos + 32];
        char nm[64] = "";
        if (nl < sizeof nm) {
            memcpy(nm, buf + pos + 33, nl); nm[nl] = 0;
            char *semi = strchr(nm, ';'); if (semi) *semi = 0;
            if (!strcasecmp(nm, name)) {
                *out_lba = le32(buf + pos + 2);
                *out_size = le32(buf + pos + 10);
                *is_dir = (buf[pos + 25] & 2) != 0;
                ok = 1; break;
            }
        }
        pos += len;
    }
    free(buf);
    return ok;
}

int iso_read_sfo(const char *file, char *title, int tn, char *id, int in)
{
    FILE *f = fopen(file, "rb");
    if (!f) return 0;
    uint8_t pvd[SECT];
    int ok = 0;
    if (rd(f, 16, 0, pvd, SECT) && pvd[0] == 1 && !memcmp(pvd + 1, "CD001", 5)) {
        uint32_t rl = le32(pvd + 156 + 2), rs = le32(pvd + 156 + 10), gl, gs, fl, fs;
        int d;
        if (dir_find(f, rl, rs, "PS3_GAME", &gl, &gs, &d) && d &&
            dir_find(f, gl, gs, "PARAM.SFO", &fl, &fs, &d) && !d && fs > 0 && fs <= 8192) {
            uint8_t sfo[8192];
            if (rd(f, fl, 0, sfo, fs)) ok = sfo_parse(sfo, (int)fs, title, tn, id, in);
        }
    }
    fclose(f);
    return ok;
}

/* ---------- list helpers ---------- */
static Entry *add(List *l)
{
    if (l->n == l->cap) {
        int nc = l->cap ? l->cap * 2 : 64;
        Entry *p = (Entry *)realloc(l->items, nc * sizeof(Entry));
        if (!p) return NULL;
        l->items = p; l->cap = nc;
    }
    memset(&l->items[l->n], 0, sizeof(Entry));
    return &l->items[l->n++];
}

static int cmp(const void *a, const void *b) { return strcasecmp(((const Entry *)a)->title, ((const Entry *)b)->title); }

void scan_free(List lists[SRC_COUNT])
{
    for (int i = 0; i < SRC_COUNT; i++) { free(lists[i].items); lists[i].items = NULL; lists[i].n = lists[i].cap = 0; }
}

static const char *const DEVS[] = {
    "/dev_hdd0", "/dev_usb000", "/dev_usb001", "/dev_usb002", "/dev_usb003",
    "/dev_usb004", "/dev_usb005", "/dev_usb006", "/dev_usb007", NULL };

static void scan_folders(List *l, const char *dev, const char *sub)
{
    char dir[300], p[600], sfo[700];
    snprintf(p, sizeof p, "%s/%s", dev, sub);
    full(dir, sizeof dir, p);
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(p, sizeof p, "%s/%s/%s/PS3_GAME/PARAM.SFO", dev, sub, e->d_name);
        full(sfo, sizeof sfo, p);
        Entry *en = add(l);
        if (!en) break;
        en->src = SRC_MM;
        snprintf(en->path, sizeof en->path, "%s/%s/%s", dev, sub, e->d_name);
        if (!sfo_from_file(sfo, en->title, sizeof en->title, en->id, sizeof en->id)) { l->n--; continue; }
        if (!en->title[0]) snprintf(en->title, sizeof en->title, "%s", e->d_name);
        en->hash = fnv(en->id[0] ? en->id : en->path);
    }
    closedir(d);
}

static void scan_isos(List *l, const char *dev)
{
    char dir[300], p[600], fp[700];
    snprintf(p, sizeof p, "%s/PS3ISO", dev);
    full(dir, sizeof dir, p);
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (!ci_ends(e->d_name, ".iso") && !ci_ends(e->d_name, ".iso.0")) continue;
        Entry *en = add(l);
        if (!en) break;
        en->src = SRC_ISO;
        snprintf(en->path, sizeof en->path, "%s/PS3ISO/%s", dev, e->d_name);
        full(fp, sizeof fp, en->path);
        if (!iso_read_sfo(fp, en->title, sizeof en->title, en->id, sizeof en->id) || !en->title[0]) {
            snprintf(en->title, sizeof en->title, "%s", e->d_name);
            char *dot = strrchr(en->title, '.');
            if (dot) { if (ci_ends(en->title, ".0")) *dot = 0; dot = strrchr(en->title, '.'); if (dot) *dot = 0; }
        }
        en->hash = fnv(en->path);
    }
    closedir(d);
}

/* retail-style title ids: BLES12345, BCUS12345, NPUB12345 ... (NPXS = system) */
static int is_game_id(const char *id)
{
    if (strlen(id) != 9) return 0;
    if (!((id[0] == 'B' && (id[1] == 'L' || id[1] == 'C')) || (id[0] == 'N' && id[1] == 'P'))) return 0;
    if (!isupper((unsigned char)id[2]) || !isupper((unsigned char)id[3])) return 0;
    if (!strncmp(id, "NPXS", 4)) return 0;
    for (int i = 4; i < 9; i++) if (!isdigit((unsigned char)id[i])) return 0;
    return 1;
}

static int exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static void scan_installed(List *pkg, List *apps)
{
    char dir[300], p[600], fp[700];
    full(dir, sizeof dir, "/dev_hdd0/game");
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(p, sizeof p, "/dev_hdd0/game/%s/USRDIR/EBOOT.BIN", e->d_name);
        full(fp, sizeof fp, p);
        if (!exists(fp)) continue;
        if (!strncmp(e->d_name, "NPXS", 4)) continue;
        if (!strcmp(e->d_name, SELF_ID)) continue;
        char title[96] = "", id[16] = "";
        snprintf(p, sizeof p, "/dev_hdd0/game/%s/PARAM.SFO", e->d_name);
        full(fp, sizeof fp, p);
        sfo_from_file(fp, title, sizeof title, id, sizeof id);
        if (!id[0]) snprintf(id, sizeof id, "%.15s", e->d_name);
        List *l = is_game_id(id) ? pkg : apps;
        Entry *en = add(l);
        if (!en) break;
        en->src = (l == pkg) ? SRC_PKG : SRC_APP;
        snprintf(en->title, sizeof en->title, "%s", title[0] ? title : e->d_name);
        snprintf(en->id, sizeof en->id, "%s", id);
        snprintf(en->path, sizeof en->path, "/dev_hdd0/game/%s", e->d_name);
        en->hash = fnv(id);
    }
    closedir(d);
}

/* Moves ISO and PKG games into the SRC_MM list so the UI shows a single "Games" list. */
void scan_merge_games(List lists[SRC_COUNT])
{
    List *g = &lists[SRC_MM];
    for (int k = SRC_ISO; k <= SRC_PKG; k++) {
        List *l = &lists[k];
        for (int i = 0; i < l->n; i++) {
            if (g->n == g->cap) {
                int nc = g->cap ? g->cap * 2 : 16;
                Entry *p = (Entry *)realloc(g->items, (size_t)nc * sizeof(Entry));
                if (!p) return;
                g->items = p; g->cap = nc;
            }
            g->items[g->n++] = l->items[i];
        }
        l->n = 0;
    }
    if (g->n > 1) qsort(g->items, g->n, sizeof(Entry), cmp);
}

void scan_all(List lists[SRC_COUNT])
{
    scan_free(lists);
    for (int i = 0; DEVS[i]; i++) {
        scan_folders(&lists[SRC_MM], DEVS[i], "GAMES");
        scan_folders(&lists[SRC_MM], DEVS[i], "GAMEZ");
        scan_isos(&lists[SRC_ISO], DEVS[i]);
    }
    scan_installed(&lists[SRC_PKG], &lists[SRC_APP]);
    for (int i = 0; i < SRC_COUNT; i++)
        if (lists[i].n > 1) qsort(lists[i].items, lists[i].n, sizeof(Entry), cmp);
}
