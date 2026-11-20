#ifndef PAGES_H
#define PAGES_H
#include <stdint.h>
#include "gfx.h"
struct Ui;

#define MAXUSERS 4

/* Settings (persisted to settings.cfg) ---------------------------------- */
typedef struct {
    int lang;      /* 0 Arabic, 1 English */
    int bg, accent, waves, panel, dim, iconsz, iconrad;
    int anim, speed, h24, askstart, askplay;
    int users, cur_user;
} Settings;
void settings_defaults(Settings *s);
void settings_theme_defaults(Settings *s);
int  settings_load(Settings *s, const char *path);
int  settings_save(const Settings *s, const char *path);

/* Files ------------------------------------------------------------------ */
#define FILES_MAX 400
typedef struct { char name[96]; int dir, sel; long long size; } FileItem;
typedef struct {
    char cwd[256];               /* "" = virtual root (list of devices) */
    FileItem *items; int n, sel, top;
} Files;
void files_open(Files *f, const char *cwd);
void files_free(Files *f);
int  files_enter(Files *f);
int  files_up(Files *f);
int  files_nsel(const Files *f);
void files_select_all(Files *f, int on);
int  files_delete(Files *f);     /* deletes marked items, or the current one; returns number of failures */
const FileItem *files_cur(const Files *f);

/* Store (local catalog file) --------------------------------------------- */
typedef struct { char name[48], desc[96], kind[24], size[16]; uint32_t hash; } StoreItem;
typedef struct { StoreItem *items; int n, sel, filter; int vis[64], nvis; } Store;
void store_load(Store *s, const char *path);
void store_free(Store *s);
void store_refilter(Store *s);   /* filter: 0 all, 1 games, 2 apps */

/* Pages (input + drawing) ------------------------------------------------ */
void page_settings_input(struct Ui *u, int l, int r, int up, int dn, int ok, int back);
void page_settings_draw(struct Ui *u, Surf *fb);
void page_media_input(struct Ui *u, int l, int r, int ok);
void page_media_draw(struct Ui *u, Surf *fb);
void page_files_draw(struct Ui *u, Surf *fb);
void page_store_draw(struct Ui *u, Surf *fb);
void page_clock(char *out, int n, int h24);
void ui_text_fit(char *out, int n, const char *in, const Font *f, int maxw);
#endif
